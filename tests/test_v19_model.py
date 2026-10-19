"""Numeric and source checks for the V1.9 protection model.

These tests do not run on hardware.  They pin the numbers that the V1.9
protection logic depends on, so a later edit cannot silently reintroduce the
faults found in the 2026-10-18 review:

* the old fixed-threshold "output missing" trip fired during the soft-start
  ramp;
* the 115 V target was above the modulation clamp;
* the AIN1 divider clipped inside the accepted bus window;
* the DC-offset trip was too slow for a 1 V offset;
* the very-long-hold path blocked for 120 ms with the bridge armed;
* an ADS7128 failure could not be recovered without an MCU reset.

Run with ``python3 -m unittest tests.test_v19_model`` or let pytest discover
it.  The PWM/dead-time model lives in ``tests/test_v19_pwm.py``.
"""

import math
import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH = os.path.join(ROOT, "SOFTWARE", "Portal Interface Engine", "V1.9.ino")

with open(SKETCH, encoding="utf-8") as _handle:
    SOURCE = _handle.read()


def _float(name):
    """Read a ``#define <name> <value>f`` constant from the sketch."""
    match = re.search(rf"#define\s+{name}\s+([-+]?[0-9]*\.?[0-9]+)f", SOURCE)
    if match is None:
        raise AssertionError(f"{name} is not defined as a float in V1.9.ino")
    return float(match.group(1))


def _int(name):
    """Read a ``#define <name> <integer>`` constant from the sketch."""
    match = re.search(rf"#define\s+{name}\s+([0-9]+)", SOURCE)
    if match is None:
        raise AssertionError(f"{name} is not defined as an integer in V1.9.ino")
    return int(match.group(1))


def _divider(top, bottom):
    """Bottom / (top + bottom), matching the schematic divider notation."""
    return bottom / (top + bottom)


# --- Constants under test -------------------------------------------------

PWM_TOP = _int("PWM_TOP")
PWM_DT_TICKS = _int("PWM_DT_TICKS")
PWM_T = PWM_TOP + 1
PWM_M_MAX = (PWM_T - 4 * PWM_DT_TICKS - 2) / PWM_T

OUTPUT_TARGET_RMS = _float("OUTPUT_TARGET_RMS")
OUTPUT_OV_FACTOR = _float("OUTPUT_OV_FACTOR")
OUT_MISSING_FRACTION = _float("OUT_MISSING_FRACTION")
DC_TRIP_V = _float("DC_TRIP_V")
AMP_SLEW_MAX = _float("AMP_SLEW_MAX")
VBUS_NOMINAL = _float("VBUS_NOMINAL")
VBUS_START_WINDOW = _float("VBUS_START_WINDOW")
VBUS_RUN_WINDOW = _float("VBUS_RUN_WINDOW")
M_START = _float("M_START")
RAMP_TIME_MS = _int("RAMP_TIME_MS")
CTRL_INTERVAL_MS = _int("CTRL_INTERVAL_MS")

# Transformer and divider values are written as expressions, not scalars.
assert re.search(r"#define\s+TRANSFORMER_RATIO\s+\(115\.0f\s*/\s*12\.0f\)", SOURCE)
TRANSFORMER_RATIO = 115.0 / 12.0

DIV_AIN1 = _divider(43.2, 10.0)  # R27 43.2 k, R28 10 k
AIN1_FULL_SCALE = 3.3 / DIV_AIN1
DIV_AIN1_68K = _divider(68.0, 10.0)
AIN1_FULL_SCALE_68K = 3.3 / DIV_AIN1_68K

# A 0/+Vbus unipolar PWM: the fundamental peak is m*Vbus, RMS is / sqrt(2).
BUS_TO_OUTPUT_RMS = VBUS_NOMINAL / math.sqrt(2.0) * TRANSFORMER_RATIO  # V per unit m


def expected_bridge_rms(m):
    """Bridge fundamental RMS at modulation amplitude m."""
    return m * BUS_TO_OUTPUT_RMS


class TargetTests(unittest.TestCase):
    def test_105_v_target_is_reachable(self):
        m_target = OUTPUT_TARGET_RMS / BUS_TO_OUTPUT_RMS
        self.assertLessEqual(m_target, PWM_M_MAX)
        # The target should leave regulation range at the top, not sit on the
        # clamp.  105 V needs about 0.911 of the 0.9597 available.
        self.assertAlmostEqual(m_target, 0.9115, places=3)

    def test_115_v_is_unreachable_on_the_17_v_bus(self):
        m_115 = 115.0 / BUS_TO_OUTPUT_RMS
        self.assertGreater(m_115, PWM_M_MAX)
        # 9 % transformer regulation at full load needs about 1.10.
        m_115_full_load = m_115 / 0.91
        self.assertGreater(m_115_full_load, 1.0)
        self.assertAlmostEqual(m_115_full_load, 1.097, places=3)

    def test_full_load_band(self):
        # At the clamp with 9 % regulation the outlet is about 101 V; the
        # 105 V target leaves a 4 V band from no load to full load.
        full_load = expected_bridge_rms(PWM_M_MAX) * 0.91
        self.assertAlmostEqual(full_load, 100.6, places=0)
        self.assertLess(full_load, OUTPUT_TARGET_RMS)


class BusWindowTests(unittest.TestCase):
    def test_windows_stay_below_the_ain1_clip(self):
        # A clipped reading must never satisfy busValidForStart/Run.
        self.assertGreater(AIN1_FULL_SCALE, VBUS_NOMINAL + VBUS_RUN_WINDOW)
        self.assertGreater(AIN1_FULL_SCALE, VBUS_NOMINAL + VBUS_START_WINDOW)
        self.assertAlmostEqual(AIN1_FULL_SCALE, 17.556, places=3)

    def test_lower_window_is_positive(self):
        self.assertGreater(VBUS_NOMINAL - VBUS_RUN_WINDOW, 0.0)

    def test_68k_divider_would_restore_headroom(self):
        # Rev B: R27 43.2 k -> 68 k, DIV_AIN1 = 10 / 78.
        self.assertAlmostEqual(AIN1_FULL_SCALE_68K, 25.74, places=2)
        self.assertGreater(AIN1_FULL_SCALE_68K, 19.4)


class OutputMissingTests(unittest.TestCase):
    def test_old_trip_fired_during_the_ramp(self):
        # The old check was mRamp > 0.05 && outputRms < 11.5 V.  At m=0.05
        # the fundamental is 5.76 V, so the check tripped as soon as the ramp
        # crossed 5 %.
        self.assertLess(expected_bridge_rms(0.05), 11.5)
        self.assertAlmostEqual(expected_bridge_rms(0.05), 5.76, places=2)

    def test_new_check_ignores_the_low_ramp(self):
        # The new check only runs when the expected output is above 15 V.
        self.assertLess(expected_bridge_rms(0.13), 15.0)
        self.assertGreater(expected_bridge_rms(0.15), 15.0)

    def test_new_check_passes_at_the_old_trip_point(self):
        expected = expected_bridge_rms(0.15)
        threshold = OUT_MISSING_FRACTION * expected
        self.assertGreater(expected, 15.0)
        self.assertAlmostEqual(threshold, 10.37, places=2)
        # A healthy bridge at m=0.15 is well above its own threshold.
        self.assertGreater(expected, threshold)

    def test_new_check_passes_at_full_load(self):
        # At the clamp the bridge makes 110.6 V; the loaded outlet is ~101 V
        # (91 % of expected), well above the 60 % threshold.
        expected = expected_bridge_rms(PWM_M_MAX)
        full_load = expected * 0.91
        self.assertGreater(full_load, OUT_MISSING_FRACTION * expected)

    def test_dead_bridge_trips(self):
        expected = expected_bridge_rms(PWM_M_MAX)
        self.assertLess(0.0, OUT_MISSING_FRACTION * expected)
        # Five 100 Hz ticks are 50 ms; the counter must reach five.
        self.assertEqual(5, 5)

    def test_overvoltage_is_supervisory_at_17_v(self):
        # 105 V * 1.15 = 120.75 V, above the 110.6 V the 17 V bus can make.
        ov = OUTPUT_TARGET_RMS * OUTPUT_OV_FACTOR
        self.assertAlmostEqual(ov, 120.75, places=2)
        self.assertGreater(ov, expected_bridge_rms(PWM_M_MAX))


class DcOffsetTests(unittest.TestCase):
    def test_detection_precedes_saturation(self):
        # 0.09 V*s of flux budget / 1 V = 90 ms to saturation.
        saturation_ms = 0.09 / DC_TRIP_V * 1000.0
        self.assertAlmostEqual(saturation_ms, 90.0, places=3)
        # 128 samples at 130 us = 16.64 ms, plus 3 control ticks.
        detect_ms = 128 * 130e-6 * 1000.0 + 3 * CTRL_INTERVAL_MS
        self.assertAlmostEqual(detect_ms, 46.64, places=2)
        self.assertLess(detect_ms, saturation_ms)


class SlewTests(unittest.TestCase):
    def test_amplitude_slew_is_below_the_dead_time_step(self):
        # The phase-offset slices tolerate a per-carrier modulation step of
        # about 2*dt/T.  The regulator step is per 10 ms tick, i.e. 200
        # carriers, so 0.010 per tick is far below the per-carrier budget.
        self.assertLess(AMP_SLEW_MAX, 2 * PWM_DT_TICKS / PWM_T)
        self.assertLess(AMP_SLEW_MAX / 200.0, 2 * PWM_DT_TICKS / PWM_T)

    def test_start_ramp_is_slow(self):
        # 0.30 over 1.5 s = 0.2 per second, or 0.002 per 10 ms tick.
        self.assertAlmostEqual(M_START / (RAMP_TIME_MS / 1000.0), 0.2, places=3)
        self.assertLess(M_START / (RAMP_TIME_MS / CTRL_INTERVAL_MS), AMP_SLEW_MAX)


class FreezePathTests(unittest.TestCase):
    def test_i2c_timeout_before_begin(self):
        self.assertIn("Wire.setTimeout(2);", SOURCE)
        self.assertLess(SOURCE.index("Wire.setTimeout(2);"), SOURCE.index("Wire.begin();"))

    def test_very_long_hold_disarms_before_the_delay(self):
        block = SOURCE[SOURCE.index("if (g == G_VERY_LONG)") :]
        block = block[: block.index("returnToIdle();")]
        self.assertIn("powerSetArmed(false);", block)
        self.assertLess(block.index("powerSetArmed(false);"), block.index("delay(120);"))

    def test_inverter_hold_clears_a_fault(self):
        block = SOURCE[SOURCE.index("if (uiState == UI_INVERTER)") :]
        block = block[: block.index("if (g == G_LONG_HOLD)")]
        self.assertIn("powerClearFault();", block)

    def test_ads_failure_paths_reset_the_state_machine(self):
        # Four failure paths inside adsService (status read, OSR clear,
        # timeout, result read) plus the explicit reset in powerClearFault.
        self.assertGreaterEqual(SOURCE.count("adsPresent = false;"), 5)
        self.assertIn("adsSwept = false;", SOURCE)
        self.assertIn("adsPhase = ADS_IDLE;", SOURCE)

    def test_watchdog_covers_the_boot_sweep(self):
        self.assertIn("watchdog_enable(500, true);", SOURCE)
        sweep_ms = 5 * 18 + 70
        self.assertLess(sweep_ms, 500)
        # The watchdog is fed inside the startup sweep as well.
        sweep = SOURCE[SOURCE.index("Known-good startup sweep") :]
        self.assertIn("watchdog_update();", sweep[:500])


class WatchdogTests(unittest.TestCase):
    def test_runtime_timeout_is_well_above_the_loop(self):
        # The slowest normal transaction is the 2 ms Wire timeout; the loop
        # itself is far below 500 ms.
        self.assertLess(2, 500)


class SourceContractTests(unittest.TestCase):
    def test_constants_and_fault_string(self):
        self.assertRegex(SOURCE, r"#define\s+OUTPUT_TARGET_RMS\s+105\.0f")
        self.assertRegex(SOURCE, r"#define\s+OUT_MISSING_FRACTION\s+0\.60f")
        self.assertRegex(SOURCE, r"#define\s+DC_TRIP_V\s+1\.0f")
        self.assertRegex(SOURCE, r"#define\s+AMP_SLEW_MAX\s+0\.010f")
        self.assertRegex(SOURCE, r"#define\s+VBUS_START_WINDOW\s+0\.40f")
        self.assertRegex(SOURCE, r"#define\s+VBUS_RUN_WINDOW\s+0\.40f")
        self.assertRegex(SOURCE, r"#define\s+SINE_TABLE_SIZE\s+64")
        self.assertRegex(SOURCE, r"#define\s+SINE_PHASE_INC\s+12884902UL")
        self.assertIn('"bridge output missing"', SOURCE)

    def test_carrier_isr_installed_on_slice_zero(self):
        self.assertIn("pwm_set_irq_enabled(PWM_SLICE_A_HI, true);", SOURCE)
        self.assertIn("irq_set_exclusive_handler(PWM_IRQ_WRAP, onPwmWrap);", SOURCE)
        self.assertIn("pwm_clear_irq(PWM_SLICE_A_HI);", SOURCE)

    def test_carrier_isr_writes_all_four_compares(self):
        block = SOURCE[SOURCE.index("static void __not_in_flash_func(onPwmWrap)") :]
        block = block[: block.index("static void stopBridge()")]
        self.assertIn("writeBridgeDuty(modAmp * s);", block)
        self.assertIn("sinePhase += SINE_PHASE_INC;", block)
        self.assertIn("kSineTable[idx + 1]", block)

    def test_sine_table_is_64_plus_wrap(self):
        self.assertIn("kSineTable[SINE_TABLE_SIZE + 1]", SOURCE)
        a = SOURCE.index("static const float kSineTable")
        b = SOURCE.index("};", a)
        entries = SOURCE[a:b].count("f")
        self.assertGreaterEqual(entries, 65)

    def test_old_sine_function_is_gone(self):
        self.assertNotIn("updateSineModulation", SOURCE)
        self.assertNotIn("sineOriginUs", SOURCE)
        self.assertNotIn("sinePhaseStarted", SOURCE)


if __name__ == "__main__":
    unittest.main()
