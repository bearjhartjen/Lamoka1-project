"""Protection-latency, measurement and arming-contract checks for V1.9.

The 2026-10-22 revision replaced the DC estimator.  The old estimator
differenced two HELD midpoint values over a sample-counted window, so the
60 Hz fundamental aliased against the ADS7128 sweep and the window could run
longer than one mains period.  Both effects put a false DC offset above the
0.5 V trip.  The estimator is now a per-channel least-squares fit of

    v(t) = DC + a*sin(theta) + b*cos(theta)

over the last mains period, using the carrier phase captured when each
conversion started.  This file models that fit independently and pins:

* the fit's exact rejection of a 60 Hz fundamental, even on a long or
  ragged window that defeats a plain mean;
* the fit's validity rules (sample count, phase span, conditioning);
* the DC-offset trip latency against the transformer's 11/X ms saturation
  knee, for the offsets the windowed estimate can actually resolve;
* the supervision added in the same revision: stale ADS channels, a fit
  that outlives its data, implausible temperatures, and the debounces;
* the arming contract: 400 kHz only, idempotent arm, disarm on leaving the
  portal, and the 100 kHz fallback being detection-only.

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


# --- constants the model and the source contract depend on -------------------
RMS_RING_SIZE = _int("RMS_RING_SIZE")
RMS_MIN_SAMPLES = _int("RMS_MIN_SAMPLES")
RMS_MIN_SPAN_US = _int("RMS_MIN_SPAN_US")
RMS_WINDOW_US = _int("RMS_WINDOW_US")
RMS_STALE_US = _int("RMS_STALE_US")
RMS_INVALID_TRIP_MS = _int("RMS_INVALID_TRIP_MS")
CTRL_INTERVAL_MS = _int("CTRL_INTERVAL_MS")
DC_TRIP_V = _float("DC_TRIP_V")
DC_TRIP_SAMPLES = _int("DC_TRIP_SAMPLES")
BUS_RUN_GRACE_TICKS = _int("BUS_RUN_GRACE_TICKS")
OUTPUT_OV_TICKS = _int("OUTPUT_OV_TICKS")
TEMP_TRIP_TICKS = _int("TEMP_TRIP_TICKS")
ADS_STALE_US = _int("ADS_STALE_US")

MAINS_HZ = 60.0
# Differential amplitude at the ADC: m ~ 0.94, 21.75 V bus, 10/78 divider,
# with the two midpoints in anti-phase.  Each channel carries half of it.
CHANNEL_PEAK_V = 0.94 * 21.75 * (10.0 / 78.0)
DIFFERENTIAL_PEAK_V = 2.0 * CHANNEL_PEAK_V

# The flux re-derivation puts the saturation knee at 11/X ms for an X-volt
# offset at full modulation (flux budget about 0.011 V.s, per the sketch's
# own DC_TRIP_V comment -- pinned, not hardcoded).
_knee = re.search(r"flux budget of about (0\.0\d+) V\.s", SOURCE)
assert _knee, "flux budget comment missing from sketch"
SATURATION_KNEE_MS = float(_knee.group(1)) * 1000.0
assert abs(SATURATION_KNEE_MS - 11.0) < 0.05, SATURATION_KNEE_MS


# --- an independent model of the firmware's fit ------------------------------

class Sample:
    __slots__ = ("t_us", "v", "s", "c")

    def __init__(self, t_us, v, phase):
        self.t_us = t_us
        self.v = v
        self.s = math.sin(phase)
        self.c = math.cos(phase)


def fit_samples(samples, now_us):
    """The firmware's three-parameter least-squares fit, in Python.

    Returns ``(dc, a, b)`` or ``None`` when the firmware would reject the
    set.  The checks mirror fitMidRing(): a time window of RMS_WINDOW_US,
    a minimum sample count, a minimum phase span, and a determinant floor
    relative to n**3.
    """
    used = [s for s in samples if 0 <= now_us - s.t_us <= RMS_WINDOW_US]
    if len(used) < RMS_MIN_SAMPLES:
        return None
    newest = used[-1].t_us
    oldest = used[0].t_us
    if newest - oldest < RMS_MIN_SPAN_US:
        return None

    n = float(len(used))
    ss = sum(s.s for s in used)
    sc = sum(s.c for s in used)
    sss = sum(s.s * s.s for s in used)
    scc = sum(s.c * s.c for s in used)
    ssc = sum(s.s * s.c for s in used)
    sy = sum(s.v for s in used)
    sys = sum(s.v * s.s for s in used)
    syc = sum(s.v * s.c for s in used)

    det = (
        n * (sss * scc - ssc * ssc)
        - ss * (ss * scc - ssc * sc)
        + sc * (ss * ssc - sss * sc)
    )
    if abs(det) < 1e-3 * n * n * n:
        return None

    dc = (
        sy * (sss * scc - ssc * ssc)
        - ss * (sys * scc - ssc * syc)
        + sc * (sys * ssc - sss * syc)
    ) / det
    a = (
        n * (sys * scc - ssc * syc)
        - sy * (ss * scc - ssc * sc)
        + sc * (ss * syc - sys * sc)
    ) / det
    b = (
        n * (sss * syc - ssc * sys)
        - ss * (ss * syc - sys * sc)
        + sy * (ss * ssc - sss * sc)
    ) / det
    return dc, a, b


def channel_samples(
    phase0=0.0,
    dc=0.0,
    step_v=0.0,
    step_us=None,
    start_us=-50000,
    stop_us=50000,
    period_us=1140.0,
    offset_us=0.0,
    sign=1.0,
    jitter_us=0.0,
):
    """One midpoint channel, sampled at the weighted-sweep cadence."""
    out = []
    t = start_us + offset_us
    k = 0
    while t <= stop_us:
        jitter = 0.0
        if jitter_us:
            jitter = ((k * 37) % 11 - 5) / 5.0 * jitter_us
        t_sample = t + jitter
        v = dc + sign * CHANNEL_PEAK_V * math.sin(phase0 + 2.0 * math.pi * MAINS_HZ * t_sample * 1e-6)
        if step_us is not None and t_sample >= step_us:
            v += step_v
        out.append(
            Sample(t_sample, v, phase0 + 2.0 * math.pi * MAINS_HZ * t_sample * 1e-6)
        )
        t += period_us
        k += 1
    return out


def differential_fit(now_us, a, b):
    fa = fit_samples(a, now_us)
    fb = fit_samples(b, now_us)
    if fa is None or fb is None:
        return None
    return fa[0] - fb[0], fa[1] - fb[1], fa[2] - fb[2]


class FitModelTests(unittest.TestCase):
    """The fit itself: exact fundamental rejection and validity rules."""

    def test_the_fit_recovers_dc_and_fundamental_from_uneven_samples(self):
        a = channel_samples(dc=0.7, jitter_us=90.0)
        b = channel_samples(
            dc=0.0, offset_us=570.0, sign=-1.0, jitter_us=70.0
        )
        got = differential_fit(33333, a, b)
        self.assertIsNotNone(got)
        dc, x, y = got
        self.assertAlmostEqual(dc, 0.7, places=3)
        # The differential fundamental is 2 * CHANNEL_PEAK_V peak.
        self.assertAlmostEqual(math.hypot(x, y), DIFFERENTIAL_PEAK_V, places=2)

    def test_the_fit_cancels_the_fundamental_on_a_long_ragged_window(self):
        # The 2026-10-21 regression: a count-based window ran 21-26 ms, so
        # the mean of a 60 Hz signal over it was not zero.  The fit removes
        # the fundamental over the SAME 26 ms of samples.
        window_us = 26000.0
        phase = 0.0
        mean = 0.0
        for k in range(64):
            t = (k + 0.5) / 64.0 * window_us
            mean += DIFFERENTIAL_PEAK_V * math.sin(
                2.0 * math.pi * MAINS_HZ * t * 1e-6
            )
        mean /= 64.0
        self.assertGreater(abs(mean), DC_TRIP_V)

        a = channel_samples(jitter_us=120.0)
        b = channel_samples(offset_us=570.0, sign=-1.0, jitter_us=80.0)
        got = differential_fit(40000, a, b)
        self.assertIsNotNone(got)
        self.assertAlmostEqual(got[0], 0.0, places=3)
        # The plain 26 ms mean asserted above is what the old count-based
        # window computed; it reads over a volt of DC that is not there.
        # The anti-phase stream is not a fair test of the mean (its two
        # channels cancel by construction), so it is not used here.

    def test_too_few_samples_are_rejected(self):
        a = channel_samples(start_us=-1000, stop_us=2000, period_us=1140.0)
        b = channel_samples(
            start_us=-1000, stop_us=2000, period_us=1140.0, sign=-1.0
        )
        self.assertIsNone(fit_samples(a, 5000))
        self.assertLess(len(a), RMS_MIN_SAMPLES)
        self.assertIsNone(fit_samples(b, 5000))

    def test_too_little_phase_span_is_rejected(self):
        # Four samples inside 3 ms: enough count, not enough phase.
        a = [
            Sample(t, math.sin(2 * math.pi * MAINS_HZ * t * 1e-6),
                   2 * math.pi * MAINS_HZ * t * 1e-6)
            for t in (0.0, 1000.0, 2000.0, 3000.0)
        ]
        self.assertIsNone(fit_samples(a, 3000.0))
        self.assertLess(3000, RMS_MIN_SPAN_US)

    def test_a_collapsed_phase_set_is_rejected_by_the_determinant(self):
        # Four samples at the same phase span enough TIME but carry no
        # phase information: the normal matrix is singular and the
        # determinant floor rejects it rather than returning a wild DC.
        a = [
            Sample(t, 1.0 + 0.01 * k, 0.3)
            for k, t in enumerate((0.0, 13000.0, 14000.0, 15000.0))
        ]
        self.assertIsNone(fit_samples(a, 15000.0))

    def test_the_fit_uses_the_phase_at_conversion_start(self):
        block = SOURCE[SOURCE.index("static bool adsStartConversion") :]
        block = block[: block.index("static void adsFail")]
        self.assertIn("adsSamplePhase = sinePhase;", block)
        # And the ring stores that phase with the sample, not the read time.
        push = SOURCE[SOURCE.index("static void pushMidSample") :]
        push = push[: push.index("static bool adsStartConversion")]
        self.assertIn("sineAt(phase)", push)
        self.assertIn("sineAt(phase + 0x40000000UL)", push)

    def test_the_weighted_sweep_favours_the_midpoints(self):
        block = SOURCE[SOURCE.index("static const uint8_t kAdsSchedule") :]
        block = block[: block.index("};")]
        values = [int(v) for v in re.findall(r"\b([0-4])\b", block)]
        self.assertEqual(len(values), 15)
        self.assertEqual(sum(1 for v in values if v in (2, 3)), 12)
        self.assertEqual(set(values), {0, 1, 2, 3, 4})


class DcOffsetLatencyTests(unittest.TestCase):
    """The trip has to arrive before the transformer saturates."""

    def _trip_time_ms(self, offset_v):
        a = channel_samples(step_v=offset_v, step_us=0.0)
        b = channel_samples(offset_us=570.0, sign=-1.0)
        # updateRmsAndDc() runs on every new midpoint conversion.  With the
        # weighted schedule a combined differential fit is available every
        # 570 us.
        consecutive = 0
        for event in range(0, 30000, 570):
            got = differential_fit(event, a, b)
            if got is None:
                consecutive = 0
                continue
            if abs(got[0]) > DC_TRIP_V:
                consecutive += 1
                if consecutive >= DC_TRIP_SAMPLES:
                    return event / 1000.0
            else:
                consecutive = 0
        return math.inf

    def test_the_trip_beats_saturation_for_resolvable_offsets(self):
        # 0.55 V is just above the threshold; 2 V is the largest step a
        # windowed 60 Hz estimate can still resolve inside the flux budget.
        for offset in (0.55, 0.75, 1.0, 2.0):
            with self.subTest(offset=offset):
                trip = self._trip_time_ms(offset)
                knee = SATURATION_KNEE_MS / offset
                self.assertLess(trip, knee)
                self.assertGreater(knee - trip, 0.5)

    def test_the_one_volt_case_has_about_two_milliseconds_of_margin(self):
        trip = self._trip_time_ms(1.0)
        knee = SATURATION_KNEE_MS
        self.assertAlmostEqual(trip, 9.1, delta=0.6)
        self.assertGreater(knee - trip, 1.5)

    def test_very_large_steps_are_documented_as_supervisory_only(self):
        # 5 V and 10 V saturate in 2.2 ms and 1.1 ms, faster than any windowed
        # 60 Hz estimate can resolve.  The sketch has to say so rather than
        # claim a detection it cannot make.
        self.assertIn("A 5 V or 10 V step saturates in 2.2 ms or 1.1 ms", SOURCE)
        self.assertIn("supervisory-only", SOURCE)
        self.assertGreaterEqual(self._trip_time_ms(5.0), 11.0 / 5.0)

    def test_the_trip_is_evaluated_on_every_fit(self):
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ==")]
        self.assertIn("if (!dcSuspect && fabsf(primaryDc) > DC_TRIP_V)", block)
        self.assertIn('setFault("DC offset")', block)
        control = SOURCE[SOURCE.index("static void runControl(") :]
        control = control[: control.index("void powerBegin()")]
        self.assertNotIn('setFault("DC offset")', control)

    def test_the_measurement_window_is_cleared_between_runs(self):
        for func in ("static void stopBridge()", "static void startBridge()"):
            block = SOURCE[SOURCE.index(func) :]
            block = block[: block.index("}", block.index(func) + len(func))]
            self.assertIn("resetMeasurementWindow();", block)

    def test_a_stale_fit_is_dropped(self):
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ==")]
        self.assertIn("(uint32_t)(micros() - midLastSampleUs) > RMS_STALE_US", block)
        self.assertIn("rmsValid = false;", block)
        self.assertLess(RMS_STALE_US, RMS_MIN_SPAN_US)

    def test_a_run_without_a_measurement_faults(self):
        self.assertIn('setFault("no drive measurement")', SOURCE)
        self.assertIn("RMS_INVALID_TRIP_MS", SOURCE)
        self.assertAlmostEqual(RMS_INVALID_TRIP_MS, 500.0)
        self.assertGreater(RMS_INVALID_TRIP_MS, 10 * CTRL_INTERVAL_MS)


class SensorSupervisionTests(unittest.TestCase):
    """Present but stale is not the same as OK."""

    def test_sensors_ok_requires_the_high_speed_bus(self):
        block = SOURCE[SOURCE.index("void powerService(") :]
        block = block[: block.index("  // Disarmed")]
        self.assertIn("adsPresent && adsClockOk && adsSwept", block)

    def test_sensors_ok_requires_every_channel_fresh(self):
        block = SOURCE[SOURCE.index("void powerService(") :]
        block = block[: block.index("  // Disarmed")]
        for ch in range(5):
            with self.subTest(channel=ch):
                self.assertIn(f"adsFresh({ch}, sensorNowUs)", block)
        self.assertIn("tempCtrlValid && tempInvValid", block)

    def test_a_lost_channel_faults_while_running(self):
        self.assertIn('setFault("sensor stale")', SOURCE)
        start = SOURCE.index("bool adsAlive")
        block = SOURCE[start : SOURCE.index('setFault("sensor stale")', start)]
        self.assertIn("adsFresh(0, nowUs)", block)
        self.assertIn("adsFresh(4, nowUs)", block)
        # The liveness check runs before the rail/bus comparisons so a dead
        # bus is attributed to the sensor, but it only faults while driving.
        self.assertIn(
            "if ((pState == POWER_RUNNING || pState == POWER_STARTING) && !adsAlive)",
            block,
        )
        control = SOURCE[SOURCE.index("static void runControl(") :]
        control = control[: control.index("void powerBegin()")]
        self.assertLess(
            control.index("bool adsAlive"), control.index("busValidForRun()")
        )

    def test_the_freshness_limit_is_many_sweeps(self):
        # Host-model cadence: the slow channels are read once per 15-step
        # sweep (~12.6 ms) and each midpoint about every 2.1 ms, so 50 ms is
        # several complete sweeps.
        self.assertEqual(ADS_STALE_US, 50000)
        self.assertGreater(ADS_STALE_US, 3 * 12600)

    def test_a_100_khz_bus_can_be_detected_but_not_armed(self):
        block = SOURCE[SOURCE.index("void powerBegin() {") :]
        block = block[: block.index("void powerService(")]
        self.assertIn("adsProbe(true)", block)
        self.assertIn("ADS7128 only at 100 kHz; arming disabled", block)
        self.assertIn("adsClockOk", block)
        arm = SOURCE[SOURCE.index("bool powerSetArmed(bool on)") :]
        arm = arm[: arm.index("void powerSerialReport()")]
        self.assertIn("ARM refused: I2C not at 400 kHz", arm)

    def test_the_retry_tries_400_khz_first(self):
        block = SOURCE[SOURCE.index("void powerService(") :]
        block = block[: block.index("  adsService();")]
        self.assertIn("Wire.setClock(400000);", block)
        self.assertLess(
            block.index("Wire.setClock(400000);"), block.index("Wire.setClock(100000);")
        )
        self.assertIn("!adsClockOk", block)

    def test_temperature_readings_are_plausibility_checked(self):
        block = SOURCE[SOURCE.index("static bool tempPlausible") :]
        block = block[: block.index("static void readTemperatures")]
        self.assertIn("TEMP_VALID_MIN_C", block)
        self.assertIn("TEMP_VALID_MAX_C", block)
        read = SOURCE[SOURCE.index("static void readTemperatures") :]
        read = read[: read.index("// Least-squares fit")]
        self.assertIn("tempPlausible", read)

    def test_over_temperature_and_output_overvoltage_are_debounced(self):
        self.assertEqual(TEMP_TRIP_TICKS, 3)
        self.assertEqual(OUTPUT_OV_TICKS, 2)
        control = SOURCE[SOURCE.index("static void runControl(") :]
        control = control[: control.index("void powerBegin()")]
        self.assertIn("tempCtrlOverCount >= TEMP_TRIP_TICKS", control)
        self.assertIn("tempInvOverCount >= TEMP_TRIP_TICKS", control)
        self.assertIn("outOvCount >= OUTPUT_OV_TICKS", control)
        self.assertIn('setFault("temperature sensor")', control)

    def test_the_debounce_counters_are_reset_when_the_bridge_stops(self):
        block = SOURCE[SOURCE.index("static void stopBridge()") :]
        block = block[: block.index("static void enablePwmSlices()")]
        for name in (
            "tempCtrlOverCount = 0;",
            "tempInvOverCount = 0;",
            "tempInvalidCount = 0;",
            "iinOverCount = 0;",
            "outOvCount = 0;",
            "busBadTicks = 0;",
            "rmsInvalidSinceMs = 0;",
        ):
            with self.subTest(name=name):
                self.assertIn(name, block)


class ArmingTests(unittest.TestCase):
    """Arming is a state transition, not a reset."""

    def test_arming_is_idempotent(self):
        arm = SOURCE[SOURCE.index("bool powerSetArmed(bool on)") :]
        arm = arm[: arm.index("void powerSerialReport()")]
        # The already-armed return comes BEFORE the arm transition, so a
        # second arm request cannot drop a RUNNING bridge to IDLE.  The
        # disarm path earlier in the function is allowed to park.
        before_reset = arm[: arm.index("pArmed = true;")]
        self.assertIn("if (pArmed) return true;", before_reset)
        after_check = arm[arm.index("if (pArmed) return true;") :]
        after_check = after_check[: after_check.index("pArmed = true;")]
        self.assertNotIn("pState = POWER_IDLE;", after_check)

    def test_disarming_is_idempotent_and_still_parks(self):
        arm = SOURCE[SOURCE.index("bool powerSetArmed(bool on)") :]
        arm = arm[: arm.index("void powerSerialReport()")]
        self.assertIn("if (!pArmed && pState != POWER_FAULT && !bridgeModulating)", arm)
        self.assertIn("stopBridge();", arm)

    def test_return_to_idle_disarms(self):
        block = SOURCE[SOURCE.index("void returnToIdle()") :]
        block = block[: block.index("void handleUI(")]
        self.assertIn("powerSetArmed(false);", block)
        self.assertLess(
            block.index("powerSetArmed(false);"), block.index("uiState = UI_MODE;")
        )

    def test_leaving_the_inverter_portal_disarms(self):
        block = SOURCE[SOURCE.index("if (uiState == UI_INVERTER)") :]
        block = block[: block.index("if (g == G_LONG_HOLD)")]
        # The G_LONG_HOLD branch follows the cut above; check the whole portal
        # block instead.
        portal = SOURCE[SOURCE.index("if (uiState == UI_INVERTER)") :]
        portal = portal[: portal.index("/* ====")]
        long_hold = portal[portal.index("if (g == G_LONG_HOLD)") :]
        self.assertIn("powerSetArmed(false);", long_hold)

    def test_a_fault_clears_the_armed_flag(self):
        block = SOURCE[SOURCE.index("static void setFault(") :]
        block = block[: block.index("// Input-rail hysteresis")]
        self.assertIn("pArmed = false;", block)
        self.assertIn("stopBridge();", block)

    def test_clear_fault_disarms_before_it_decides(self):
        block = SOURCE[SOURCE.index("void powerClearFault() {") :]
        block = block[: block.index("bool powerArmed() {")]
        before_refusal = block[: block.index("CLEAR ignored")]
        self.assertIn("pArmed = false;", before_refusal)
        self.assertIn("adsClockOk = false;", block)


class BusWindowGraceTests(unittest.TestCase):
    """The bus run window tolerates a fixed number of bad ticks."""

    def test_the_grace_is_three_ticks(self):
        self.assertEqual(BUS_RUN_GRACE_TICKS, 3)

    def test_the_grace_is_thirty_milliseconds(self):
        self.assertAlmostEqual(BUS_RUN_GRACE_TICKS * CTRL_INTERVAL_MS, 30.0)

    def test_the_grace_counter_is_file_scope_and_reset(self):
        block = SOURCE[SOURCE.index("static bool busValidForRun()") :]
        block = block[: block.index("// ADS7128 register primitives")]
        self.assertIn("if (busBadTicks < BUS_RUN_GRACE_TICKS)", block)
        self.assertIn("busBadTicks = 0;", block)
        self.assertIn("static uint8_t busBadTicks = 0;", SOURCE)


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
        self.assertIn("adsSetBits(ADS7128_GENERAL_CFG, 0x20)", SOURCE)
        self.assertIn("adsSetBits(ADS7128_GENERAL_CFG, 0x08)", SOURCE)
        self.assertIn("if (status & ADS7128_OSR_DONE)", SOURCE)
        self.assertRegex(SOURCE, r"#define\s+ADS7128_OSR_DONE\s+0x08")

    def test_osr_done_is_cleared_with_a_plain_write(self):
        block = SOURCE[SOURCE.index("if (status & ADS7128_OSR_DONE)") :]
        block = block[: block.index("adsPhase = ADS_READING;")]
        self.assertIn("adsWriteReg(ADS7128_SYSTEM_STATUS, ADS7128_OSR_DONE)", block)
        self.assertNotIn("adsSetBits(ADS7128_SYSTEM_STATUS", SOURCE)

    def test_a_missing_osr_done_does_not_fault(self):
        self.assertNotIn('setFault("ADS7128 timeout")', SOURCE)
        self.assertIn("ADS7128_OSR_TIMEOUT_US", SOURCE)
        self.assertIn("WARN: ADS7128 OSR_DONE did not assert", SOURCE)

    def test_the_result_is_assembled_as_a_left_justified_word(self):
        self.assertIn("*code = (uint16_t)(((uint16_t)msb << 8) | lsb) >> 4;", SOURCE)

    def test_the_channel_selection_uses_manual_chid(self):
        self.assertIn("adsWriteReg(ADS7128_CHANNEL_SEL, ch & 0x0F)", SOURCE)

    def test_the_append_status_field_is_cleared(self):
        self.assertIn("adsWriteReg(ADS7128_DATA_CFG, 0x00)", SOURCE)


if __name__ == "__main__":
    unittest.main()
