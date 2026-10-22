"""Protection-latency and ADS7128 register-contract checks for V1.9.

These tests were added after the 2026-10-20 audit.  They pin two things the
existing suite did not cover:

* the *latency* of the protection trips, measured from the code's own
  constants, so that a change to a window length or a debounce count cannot
  quietly move a trip further away from the fault it is meant to catch;
* the ADS7128 register contract -- opcodes, register addresses and bit
  positions -- against the values in the datasheet (TI SBAS868A), so a typo in
  an opcode or an address cannot pass unnoticed.

The DC-offset trip is the interesting one.  It has to react before the
transformer saturates, so the test compares the detection time against the
saturation knee the Rev C flux re-derivation gives (11-13 ms for a 1 V
offset) for every offset that can trip it.

Run with ``python3 -m pytest tests/test_v19_protection_latency.py``.
"""

import math
import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH = os.path.join(ROOT, "SOFTWARE", "Portal Interface Engine", "V1.9.ino")

with open(SKETCH, encoding="utf-8") as _handle:
    SOURCE = _handle.read()


def _int(name):
    match = re.search(rf"#define\s+{name}\s+([0-9]+)", SOURCE)
    if match is None:
        raise AssertionError(f"{name} is not defined as an integer in V1.9.ino")
    return int(match.group(1))


def _float(name):
    match = re.search(rf"#define\s+{name}\s+([-+]?[0-9]*\.?[0-9]+)f", SOURCE)
    if match is None:
        raise AssertionError(f"{name} is not defined as a float in V1.9.ino")
    return float(match.group(1))


# --- the constants the protection depends on ---------------------------------

RMS_RING_SIZE = _int("RMS_RING_SIZE")
RMS_MIN_SAMPLES = _int("RMS_MIN_SAMPLES")
RMS_SAMPLE_US = _int("RMS_SAMPLE_US")
RMS_WINDOW_US = _int("RMS_WINDOW_US")
CTRL_INTERVAL_MS = _int("CTRL_INTERVAL_MS")
DC_TRIP_V = _float("DC_TRIP_V")
DC_TRIP_SAMPLES = _int("DC_TRIP_SAMPLES")
BUS_RUN_GRACE_TICKS = _int("BUS_RUN_GRACE_TICKS")

# The Rev C flux re-derivation puts the saturation knee at 11-13 ms at the
# operating point (AGENTS.md, "Still open"; PIE-v1.9-information.md).  The
# 90 ms figure the sketch used to quote predates that re-derivation.
#
#   flux budget to saturation  ~0.011 V.s
#   saturation time for X       11/X ms
SATURATION_KNEE_MS = 11.0


def dc_detection_latency_ms(offset_v=1.01, window_full=True):
    """How long a step DC offset of ``offset_v`` takes to trip.

    The firmware's DC estimate is the mean of a one-mains-cycle window
    measured in TIME (``RMS_WINDOW_US``), so a step offset of X volts only
    reaches the threshold T once a fraction T/X of the window has been
    filled with the offset.  The trip then needs DC_TRIP_SAMPLES consecutive
    samples above the threshold, and the trip is evaluated on every RMS
    sample, not on the control tick.

    ``window_full`` is the normal case: the offset appears while the bridge
    is already running and the window already holds a full mains cycle.
    With ``window_full`` false the estimate is not even computed until the
    window holds a full period -- that is the start-up corner case, where
    the offset appears in the first milliseconds of a run.
    """
    if offset_v <= DC_TRIP_V:
        return math.inf  # the mean never reaches the threshold
    window_ms = RMS_WINDOW_US / 1000.0
    if not window_full:
        # The window has to fill with the offset before it can trip.
        window_ms += RMS_WINDOW_US / 1000.0
    return window_ms * (DC_TRIP_V / offset_v) + (
        DC_TRIP_SAMPLES - 1
    ) * RMS_SAMPLE_US / 1000.0


class DcOffsetLatencyTests(unittest.TestCase):
    """The DC-offset trip has to react before the transformer saturates."""

    def test_the_detection_beats_saturation_across_the_range(self):
        for offset in (0.55, 0.75, 1.0, 2.0, 5.0, 10.0):
            with self.subTest(offset=offset):
                detect = dc_detection_latency_ms(offset)
                saturate = SATURATION_KNEE_MS / offset
                self.assertLess(detect, saturate)

    def test_the_margin_at_one_volt(self):
        detect = dc_detection_latency_ms(1.0)
        saturate = SATURATION_KNEE_MS / 1.0
        self.assertAlmostEqual(detect, 8.45, delta=0.05)
        self.assertLess(detect, saturate)
        # At least 2 ms of margin at the worst realistic offset.
        self.assertGreater(saturate - detect, 2.0)

    def test_the_old_threshold_could_never_be_fast_enough(self):
        """With a 1.0 V threshold the one-mains-cycle window the DC estimate
        needs (16.667 ms) is longer than the 11 ms knee on its own, so no
        evaluation point could have beaten it.  This is why the threshold
        moved, and it is the property the change rests on."""
        window_ms = RMS_WINDOW_US / 1000.0
        self.assertGreater(window_ms, SATURATION_KNEE_MS)
        # And with the new threshold the window still fits inside the knee
        # for the offsets that trip it.
        self.assertLess(window_ms, SATURATION_KNEE_MS / DC_TRIP_V)

    def test_an_offset_exactly_at_the_threshold_never_trips(self):
        # The mean of a step offset approaches the offset but never exceeds
        # it, so the comparison must be strictly greater.  An offset below
        # the threshold does not saturate the transformer either, so this is
        # not a hole -- but it is worth pinning.
        self.assertEqual(dc_detection_latency_ms(DC_TRIP_V), math.inf)

    def test_the_trip_is_evaluated_on_every_rms_sample(self):
        """Not on the 100 Hz control tick: that alone would add 10 ms, which
        is most of the saturation window."""
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ==")]
        self.assertIn("if (fabsf(primaryDc) > DC_TRIP_V)", block)
        self.assertIn('setFault("DC offset")', block)
        # And it must NOT still be in the control loop.
        control = SOURCE[SOURCE.index("static void runControl(") :]
        control = control[: control.index("void powerBegin()")]
        self.assertNotIn('setFault("DC offset")', control)

    def test_the_trip_is_armed_only_once_the_window_is_full(self):
        """A part-full window is shorter than a mains cycle, so the 60 Hz
        fundamental does not cancel in it: a 12.5 ms window can have a mean
        above the trip threshold.  Evaluating the trip before the window is
        full would fire on every start-up."""
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ==")]
        self.assertIn("spanUs + RMS_SAMPLE_US < RMS_WINDOW_US", block)
        self.assertIn("rmsValid = true", block)
        self.assertNotIn("rmsCount >= RMS_RING_SIZE", block)

    def test_a_part_full_window_would_false_trip(self):
        """The arithmetic behind the rule above: a window shorter than one
        mains period leaves a residual mean of the 60 Hz fundamental.  At
        12.5 ms the worst-case mean is above the trip threshold; at a full
        period it cancels."""
        amplitude = 0.94 * 21.75 * 0.128  # differential amplitude at the ADC
        omega = 2.0 * math.pi * 60.0

        def worst_mean(length_s):
            return (amplitude / omega) * 2.0 * math.sin(omega * length_s / 2.0) / length_s

        self.assertGreater(worst_mean(0.0125), DC_TRIP_V)
        # A full window cancels the fundamental.
        self.assertLess(worst_mean(RMS_WINDOW_US * 1e-6), 0.01)
        self.assertLess(worst_mean(RMS_WINDOW_US * 1e-6), DC_TRIP_V / 10.0)

    def test_the_time_window_survives_the_real_loop_cadence(self):
        """The 2026-10-21 regression: the main loop shares the bus with the
        ADS7128, so the sample cadence is 130-260 us, not 130 us.  A
        count-based window then runs 21-26 ms and the 60 Hz residual
        false-trips the 0.5 V DC check during the soft-start ramp.  This
        models the cadence the loop actually produces and pins both the old
        failure and the new behaviour."""
        amplitude = 0.94 * 21.75 * 0.128  # differential amplitude at the ADC
        cadence_us = (190, 215, 142, 130, 260)  # one ADS7128 sweep pattern

        def samples(cycles=4):
            out = []
            t_us = 0
            i = 0
            while t_us <= cycles * RMS_WINDOW_US:
                out.append((t_us, amplitude * math.sin(2.0 * math.pi * 60.0 * t_us * 1e-6)))
                t_us += cadence_us[i % len(cadence_us)]
                i += 1
            return out

        def mean(rows):
            return sum(v for _, v in rows) / len(rows)

        rows = samples()
        # The old count-based window (128 samples) runs long and trips.  Take
        # the worst over every window position, not one lucky phase.
        old = max(
            abs(mean(rows[k - 127 : k + 1])) for k in range(128, len(rows))
        )
        self.assertGreater(old, DC_TRIP_V)

        # The new time window holds one period and does not.  Sweep the window
        # position too, and apply the firmware's own arming condition.
        new = 0.0
        for k in range(64, len(rows)):
            now = rows[k][0]
            window = [
                (t, v)
                for t, v in rows[max(0, k - 200) : k + 1]
                if now - t <= RMS_WINDOW_US * 1e-6
            ]
            if len(window) < RMS_MIN_SAMPLES:
                continue
            span = now - min(t for t, _ in window)
            if span + RMS_SAMPLE_US * 1e-6 < RMS_WINDOW_US * 1e-6:
                continue
            new = max(new, abs(sum(v for _, v in window) / len(window)))
        self.assertLess(new, DC_TRIP_V / 2.0)
        self.assertLess(new, old / 5.0)

        # And the window the code builds is long enough to be armed at all.
        self.assertGreaterEqual(RMS_RING_SIZE * RMS_SAMPLE_US, RMS_WINDOW_US)

    def test_the_measurement_window_is_cleared_between_runs(self):
        """Otherwise the tail of the previous run, and the filter's settling
        transient, would look like a DC offset."""
        for func in ("static void stopBridge()", "static void startBridge()"):
            block = SOURCE[SOURCE.index(func) :]
            block = block[: block.index("}", block.index(func) + len(func))]
            self.assertIn("resetMeasurementWindow();", block)

    def test_the_sketch_comment_does_not_claim_the_stale_figure(self):
        self.assertNotIn("~90 ms to saturate", SOURCE)
        self.assertNotIn("primary DC offset; ~90 ms", SOURCE)


class BusWindowGraceTests(unittest.TestCase):
    """The bus run window tolerates a fixed number of bad ticks."""

    def test_the_grace_is_three_ticks(self):
        self.assertEqual(BUS_RUN_GRACE_TICKS, 3)

    def test_the_grace_is_thirty_milliseconds(self):
        self.assertAlmostEqual(BUS_RUN_GRACE_TICKS * CTRL_INTERVAL_MS, 30.0)

    def test_the_code_uses_the_constant(self):
        self.assertIn("BUS_RUN_GRACE_TICKS", SOURCE)
        block = SOURCE[SOURCE.index("static bool busValidForRun()") :]
        block = block[: block.index("// ADS7128 register primitives")]
        self.assertIn("if (badTicks < BUS_RUN_GRACE_TICKS)", block)
        self.assertIn("badTicks = 0;", block)


class Ads7128ContractTests(unittest.TestCase):
    """The ADS7128 register contract, against TI SBAS868A.

    Opcodes (Table 9): 0x08 single register write, 0x10 single register read,
    0x18 set bit, 0x30 read a continuous block of registers.

    Register addresses (Table 10): SYSTEM_STATUS 0x00, GENERAL_CFG 0x01,
    DATA_CFG 0x02, CHANNEL_SEL 0x11, RECENT_CH0_LSB 0xA0.

    Bit positions (Tables 12, 13, 14, 24): OSR_DONE = bit 3 of SYSTEM_STATUS,
    STATS_EN = bit 5 and CNVST = bit 3 of GENERAL_CFG, APPEND_STATUS = bits
    5:4 of DATA_CFG, MANUAL_CHID = bits 3:0 of CHANNEL_SEL.
    """

    def test_the_opcodes_match_the_datasheet(self):
        self.assertRegex(SOURCE, r"#define\s+ADS7128_OP_WRITE\s+0x08")
        self.assertRegex(SOURCE, r"#define\s+ADS7128_OP_READ\s+0x10")
        self.assertRegex(SOURCE, r"#define\s+ADS7128_OP_SET_BITS\s+0x18")
        self.assertRegex(SOURCE, r"#define\s+ADS7128_OP_BLK_READ\s+0x30")

    def test_the_register_addresses_match_the_datasheet(self):
        self.assertRegex(SOURCE, r"#define\s+ADS7128_SYSTEM_STATUS\s+0x00")
        self.assertRegex(SOURCE, r"#define\s+ADS7128_GENERAL_CFG\s+0x01")
        self.assertRegex(SOURCE, r"#define\s+ADS7128_DATA_CFG\s+0x02")
        self.assertRegex(SOURCE, r"#define\s+ADS7128_CHANNEL_SEL\s+0x11")
        self.assertRegex(SOURCE, r"#define\s+ADS7128_RECENT_CH0_LSB\s+0xA0")

    def test_the_address_is_the_one_the_open_addr_pin_selects(self):
        # Table 2: R1 and R2 both DNP -> 001 0000b = 0x10.
        self.assertRegex(SOURCE, r"#define\s+ADS7128_ADDR\s+0x10")

    def test_the_bit_positions_match_the_datasheet(self):
        # STATS_EN (GENERAL_CFG bit 5) and CNVST (GENERAL_CFG bit 3).
        self.assertIn("adsSetBits(ADS7128_GENERAL_CFG, 0x20)", SOURCE)
        self.assertIn("adsSetBits(ADS7128_GENERAL_CFG, 0x08)", SOURCE)
        # OSR_DONE (SYSTEM_STATUS bit 3).
        self.assertIn("if (status & ADS7128_OSR_DONE)", SOURCE)
        self.assertRegex(SOURCE, r"#define\s+ADS7128_OSR_DONE\s+0x08")

    def test_osr_done_is_cleared_with_a_plain_write(self):
        """OSR_DONE is write-1-to-clear (Table 12).  The "set bit" opcode is
        documented to SET bits (8.5.2.2), which is a different operation on
        a write-1-to-clear bit -- if it set the bit instead of clearing it,
        the poll would always succeed and every channel would report the
        previous channel's conversion."""
        block = SOURCE[SOURCE.index("if (status & ADS7128_OSR_DONE)") :]
        block = block[: block.index("adsPhase = ADS_READING;")]
        self.assertIn("adsWriteReg(ADS7128_SYSTEM_STATUS, ADS7128_OSR_DONE)", block)
        self.assertNotIn("adsSetBits(ADS7128_SYSTEM_STATUS", SOURCE)

    def test_a_missing_osr_done_does_not_fault(self):
        """With OSR_CFG = 0 ("No averaging") the datasheet describes
        OSR_DONE as the averaging status, so it may never assert.  The
        conversion itself takes about 2 us, so the driver falls back to
        reading the result rather than stopping the inverter."""
        self.assertNotIn('setFault("ADS7128 timeout")', SOURCE)
        self.assertIn("ADS7128_OSR_TIMEOUT_US", SOURCE)
        self.assertIn("WARN: ADS7128 OSR_DONE did not assert", SOURCE)

    def test_the_result_is_assembled_as_a_left_justified_word(self):
        # Frame A of the datasheet: the 12-bit result occupies bits 15:4 of
        # the 16-bit word, so the code is the word shifted right by four.
        self.assertIn("*code = (uint16_t)(((uint16_t)msb << 8) | lsb) >> 4;", SOURCE)

    def test_the_channel_selection_uses_manual_chid(self):
        # CHANNEL_SEL bits 3:0 are MANUAL_CHID in manual mode, which is the
        # power-up mode of the device.
        self.assertIn("adsWriteReg(ADS7128_CHANNEL_SEL, ch & 0x0F)", SOURCE)

    def test_the_append_status_field_is_cleared(self):
        # DATA_CFG bits 5:4 = APPEND_STATUS; 00 means "not appended".
        self.assertIn("adsWriteReg(ADS7128_DATA_CFG, 0x00)", SOURCE)


if __name__ == "__main__":
    unittest.main()
