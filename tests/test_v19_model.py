"""Numeric and source checks for the V1.9 regulation and protection model.

These tests do not run on hardware.  They pin the numbers that the V1.9
protection and regulation logic depends on, so a later edit cannot silently
reintroduce the faults found in the 2026-10-18 review:

* the old fixed-threshold "output missing" trip fired during the soft-start
  ramp;
* the 120 V target was above the modulation clamp at the old 17 V bus;
* the AIN1 divider clipped inside the accepted bus window;
* the DC-offset trip was too slow for a 1 V offset;
* the very-long-hold path blocked for 120 ms with the bridge armed;
* an ADS7128 failure could not be recovered without an MCU reset.

They also pin the Rev C operating point: the 21.75 V bus, the Triad
VPT24-6670 transformer's NO-LOAD ratio (the nameplate 12.0 V is the
full-load voltage, so the open-circuit secondary is 12.96 V), the droop
compensation, the modulation ceiling and the 125 V receptacle ceiling.
The droop compensation is a feed-forward estimate of the outlet, so the
numbers below are the numbers the firmware computes at run time; they are
checked here against the closed form the firmware uses, not against a
measurement.

Run with ``python3 -m unittest tests.test_v19_model`` or let pytest discover
it.  The PWM/dead-time model lives in ``tests/test_v19_pwm.py``.
"""

import math
import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH = os.path.join(ROOT, "SOFTWARE", "Portal Interface Engine", "V1.9.ino")
SKETCH_V18 = os.path.join(ROOT, "SOFTWARE", "Portal Interface Engine", "V1.8.ino")

with open(SKETCH, encoding="utf-8") as _handle:
    SOURCE = _handle.read()

with open(SKETCH_V18, encoding="utf-8") as _handle:
    SOURCE_V18 = _handle.read()


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


def _ratio_define(name):
    """Read a ``#define <name> (a.f / b.f)`` ratio from the sketch."""
    match = re.search(
        rf"#define\s+{name}\s+\(\s*([0-9.]+)f\s*/\s*([0-9.]+)f\s*\)",
        SOURCE,
    )
    if match is None:
        raise AssertionError(f"{name} is not a ratio expression in V1.9.ino")
    return float(match.group(1)) / float(match.group(2))


def _divider(top, bottom):
    """Bottom / (top + bottom), matching the schematic divider notation."""
    return bottom / (top + bottom)


def _divider_define(name):
    """Read a ``#define <name> (a.f / (b.f + c.f))`` divider from the sketch."""
    match = re.search(
        rf"#define\s+{name}\s+\(\s*([0-9.]+)f\s*/\s*\(\s*([0-9.]+)f\s*\+\s*([0-9.]+)f\s*\)\s*\)",
        SOURCE,
    )
    if match is None:
        raise AssertionError(f"{name} is not a divider expression in V1.9.ino")
    bottom = float(match.group(1))
    top = float(match.group(2))
    return bottom / (top + bottom)


# --- Constants under test -------------------------------------------------

PWM_TOP = _int("PWM_TOP")
PWM_DT_TICKS = _int("PWM_DT_TICKS")
PWM_T = PWM_TOP + 1
PWM_M_MAX = (PWM_T - 4 * PWM_DT_TICKS - 2) / PWM_T

OUTPUT_TARGET_RMS = _float("OUTPUT_TARGET_RMS")
OUTPUT_OV_FACTOR = _float("OUTPUT_OV_FACTOR")
OUTLET_V_MAX = _float("OUTLET_V_MAX")
OUT_MISSING_FRACTION = _float("OUT_MISSING_FRACTION")
DC_TRIP_V = _float("DC_TRIP_V")
DC_TRIP_SAMPLES = _int("DC_TRIP_SAMPLES")
RMS_RING_SIZE = _int("RMS_RING_SIZE")
RMS_MIN_SAMPLES = _int("RMS_MIN_SAMPLES")
RMS_MIN_SPAN_US = _int("RMS_MIN_SPAN_US")
RMS_WINDOW_US = _int("RMS_WINDOW_US")
RMS_STALE_US = _int("RMS_STALE_US")
RMS_INVALID_TRIP_MS = _int("RMS_INVALID_TRIP_MS")
AMP_SLEW_MAX = _float("AMP_SLEW_MAX")
VBUS_NOMINAL = _float("VBUS_NOMINAL")
VBUS_START_WINDOW = _float("VBUS_START_WINDOW")
VBUS_RUN_WINDOW = _float("VBUS_RUN_WINDOW")
M_START = _float("M_START")
AMP_KI = _float("AMP_KI")
RAMP_TIME_MS = _int("RAMP_TIME_MS")
CTRL_INTERVAL_MS = _int("CTRL_INTERVAL_MS")
B_DROOP = _float("B_DROOP")
P_AUX_WATTS = _float("P_AUX_WATTS")
ETA_BUCK_HBRIDGE = _float("ETA_BUCK_HBRIDGE")
OUT_OF_REACH_TICKS = _int("OUT_OF_REACH_TICKS")
OUT_OF_REACH_BAND_V = _float("OUT_OF_REACH_BAND_V")
LOAD_LIMIT_W = _float("LOAD_LIMIT_W")
LOAD_HEAVY_TICKS = _int("LOAD_HEAVY_TICKS")

# The transformer and the droop constants are written as expressions.
# TRANSFORMER_NOLOAD_RATIO is the NO-LOAD (open-circuit) turns ratio of the
# VPT24-6670: the nameplate 12.0 V is the FULL-LOAD secondary voltage, so
# the open-circuit secondary is 12.0 x 1.08 = 12.96 V and the ratio is
# 115/12.96, not 115/12.
TRANSFORMER_NOLOAD_RATIO = _ratio_define("TRANSFORMER_NOLOAD_RATIO")
assert re.search(r"#define\s+A_DROOP\s+TRANSFORMER_NOLOAD_RATIO", SOURCE)
A_DROOP = TRANSFORMER_NOLOAD_RATIO

DIV_AIN0 = _divider_define("DIV_AIN0")
DIV_AIN1 = _divider_define("DIV_AIN1")
DIV_AIN2 = _divider_define("DIV_AIN2")
DIV_AIN3 = _divider_define("DIV_AIN3")
AIN1_FULL_SCALE = 3.3 / DIV_AIN1
AIN1_FULL_SCALE_43K2 = 3.3 / _divider(43.2, 10.0)
AIN2_FULL_SCALE_45K3 = 3.3 / _divider(45.3, 10.0)

# The design's full output power.
FULL_LOAD_W = 100.0

# The boost's tolerance band: 1 % feedback resistors and a 1.188/1.2/1.212 V
# reference around the 21.749 V nominal set by the LM5122's 1.2 V reference
# with Rfbt1 = 64.9 k and Rfbb1 = 3.79 k.
BUS_BAND_MIN = 21.1285
BUS_BAND_MAX = 22.3856

# A 0/+Vbus unipolar PWM: the fundamental peak is m*Vbus, RMS is / sqrt(2).
BUS_TO_OUTPUT_RMS = VBUS_NOMINAL / math.sqrt(2.0) * TRANSFORMER_NOLOAD_RATIO


def expected_bridge_rms(m, vbus=VBUS_NOMINAL):
    """Bridge fundamental RMS at modulation amplitude m, on the outlet scale."""
    return m * vbus / math.sqrt(2.0) * TRANSFORMER_NOLOAD_RATIO


def bridge_primary_rms(m, vbus=VBUS_NOMINAL):
    """RMS of the 12 V drive winding for a modulation amplitude m."""
    return m * vbus / math.sqrt(2.0)


def modulation_for_outlet(v_out, p_out=0.0, vbus=VBUS_NOMINAL, b_droop=B_DROOP):
    """The modulation that holds v_out at the estimated load p_out.

    This is the inverse of the firmware's droop model: the drive has to
    cover the target plus the droop of the load current.
    """
    i_sec = p_out / v_out
    x = v_out + b_droop * i_sec
    return x * math.sqrt(2.0) / (A_DROOP * vbus)


def droop_estimate(v_drive_rms, p_out, b_droop=B_DROOP, a_droop=A_DROOP):
    """The firmware's closed-form droop compensation.

    ``V_out_est = (x + sqrt(x*x - 4*y)) / 2`` with ``x = A_DROOP*V_drive``
    and ``y = B_DROOP*P_out``; the load is treated as zero when the
    discriminant is negative (the drive cannot support the load).
    """
    x = a_droop * v_drive_rms
    y = b_droop * p_out
    if y <= 0.0:
        return x
    discriminant = x * x - 4.0 * y
    if discriminant < 0.0:
        return x
    return 0.5 * (x + math.sqrt(discriminant))


def outlet_drive_limit(p_out, vbus=VBUS_NOMINAL, b_droop=B_DROOP, a_droop=A_DROOP):
    """The firmware's ceiling on the drive, from OUTLET_V_MAX."""
    y = b_droop * p_out
    x_max = OUTLET_V_MAX + (y / OUTLET_V_MAX if y > 0.0 else 0.0)
    return min(PWM_M_MAX, x_max * math.sqrt(2.0) / (a_droop * vbus))


def regulator_out_of_reach(
    v_drive_rms, p_out, m_target, drive_limit, a_droop=A_DROOP, b_droop=B_DROOP
):
    """The firmware's "output out of reach" test.

    The regulator is saturated when mTarget sits at the ceiling; the
    ceiling is the PWM clamp when nothing else binds; and the target is
    out of reach when the estimated outlet is still below it.
    """
    clamp_bound = drive_limit >= PWM_M_MAX - 1e-4
    at_ceiling = m_target >= drive_limit - 1e-4
    below = (
        droop_estimate(v_drive_rms, p_out, b_droop, a_droop)
        < OUTPUT_TARGET_RMS - OUT_OF_REACH_BAND_V
    )
    return clamp_bound and at_ceiling and below


def fitted_modulation_at_band_min(regulation, p_out=FULL_LOAD_W):
    """The modulation a unit with this regulation needs, with its own fit.

    With the per-unit constants (A = k, B = Z1) the modulation needed at
    the bottom of the bus band is the physical requirement.
    """
    k, z1 = transformer_model(regulation)
    i_sec = p_out / OUTPUT_TARGET_RMS if p_out > 0.0 else 0.0
    drive = (OUTPUT_TARGET_RMS + z1 * i_sec) / k
    return drive * math.sqrt(2.0) / BUS_BAND_MIN


def transformer_model(regulation):
    """Turns ratio and secondary-referred series impedance for a regulation r.

    ``regulation`` is the fraction by which the secondary rises from full
    load to no load (8 % for the VPT24-6670's catalogue figure).
    """
    k = 115.0 / (12.0 * (1.0 + regulation))
    z1 = (12.0 * regulation / 13.34) * k * k
    return k, z1


def physical_outlet(k, z1, v_drive_rms, p_out):
    """The outlet a real transformer delivers for a drive and a load."""
    i_sec = p_out / OUTPUT_TARGET_RMS if p_out > 0.0 else 0.0
    return k * v_drive_rms - z1 * i_sec


class TopologyTests(unittest.TestCase):
    """The Rev C operating point: 21.75 V bus, VPT24-6670, 120 V outlet."""

    def test_output_target_is_120_v(self):
        self.assertAlmostEqual(OUTPUT_TARGET_RMS, 120.0, places=3)

    def test_bus_is_21_75_v(self):
        self.assertAlmostEqual(VBUS_NOMINAL, 21.75, places=3)

    def test_bus_windows(self):
        # The start window has to accept the whole tolerance band
        # (±0.63 V) plus the reference tolerance: ±0.90 V.
        self.assertAlmostEqual(VBUS_START_WINDOW, 0.90, places=3)
        # The run window also absorbs the load-dump droop.
        self.assertAlmostEqual(VBUS_RUN_WINDOW, 1.50, places=3)

    def test_the_start_window_accepts_the_whole_tolerance_band(self):
        self.assertLessEqual(VBUS_NOMINAL - VBUS_START_WINDOW, BUS_BAND_MIN)
        self.assertGreaterEqual(VBUS_NOMINAL + VBUS_START_WINDOW, BUS_BAND_MAX)

    def test_the_run_window_accepts_the_whole_tolerance_band(self):
        self.assertLessEqual(VBUS_NOMINAL - VBUS_RUN_WINDOW, BUS_BAND_MIN)
        self.assertGreaterEqual(VBUS_NOMINAL + VBUS_RUN_WINDOW, BUS_BAND_MAX)

    def test_all_three_high_voltage_dividers_are_68_k(self):
        # R27 (AIN1), R29 and R31 (AIN2/AIN3) all become 68 kΩ.
        for name, value in (
            ("DIV_AIN1", DIV_AIN1),
            ("DIV_AIN2", DIV_AIN2),
            ("DIV_AIN3", DIV_AIN3),
        ):
            with self.subTest(name=name):
                self.assertAlmostEqual(value, _divider(68.0, 10.0), places=9)
        self.assertAlmostEqual(AIN1_FULL_SCALE, 25.74, places=2)

    def test_old_dividers_would_clip_on_this_bus(self):
        # The 45.3 kΩ midpoint dividers clipped at 18.25 V and the 43.2 kΩ
        # bus divider at 17.56 V — both below the 21.75 V bus.
        self.assertAlmostEqual(AIN2_FULL_SCALE_45K3, 18.249, places=3)
        self.assertLess(AIN2_FULL_SCALE_45K3, VBUS_NOMINAL)
        self.assertAlmostEqual(AIN1_FULL_SCALE_43K2, 17.556, places=3)
        self.assertLess(AIN1_FULL_SCALE_43K2, VBUS_NOMINAL)

    def test_overvoltage_factor_and_receptacle_ceiling(self):
        self.assertAlmostEqual(OUTPUT_OV_FACTOR, 1.05, places=3)
        self.assertAlmostEqual(OUTLET_V_MAX, 125.0, places=3)
        # The supervisory trip sits just above the hard ceiling.
        self.assertGreater(OUTPUT_TARGET_RMS * OUTPUT_OV_FACTOR, OUTLET_V_MAX)
        self.assertLess(OUTPUT_TARGET_RMS * OUTPUT_OV_FACTOR, 127.0)

    def test_droop_ratio_is_the_no_load_transformer_ratio(self):
        self.assertAlmostEqual(A_DROOP, TRANSFORMER_NOLOAD_RATIO, places=9)
        self.assertAlmostEqual(A_DROOP, 115.0 / 12.96, places=9)

    def test_the_no_load_ratio_is_not_the_full_load_ratio(self):
        # 115/12 = 9.5833 is the FULL-LOAD ratio: using it over-states the
        # outlet by 8 %, which is the bug this revision fixes.
        full_load_ratio = 115.0 / 12.0
        self.assertNotAlmostEqual(A_DROOP, full_load_ratio, places=3)
        self.assertAlmostEqual(A_DROOP / full_load_ratio, 0.926, places=3)
        self.assertAlmostEqual(full_load_ratio / A_DROOP, 1.08, places=3)

    def test_droop_constant_is_the_catalogue_value(self):
        # Z1 = (12.96 - 12.0)/13.34 x k^2 = 5.666 Ω for the 8 % catalogue
        # figure of the VPT24-6670.
        k, z1 = transformer_model(0.08)
        self.assertAlmostEqual(B_DROOP, z1, places=2)
        self.assertAlmostEqual(z1, 5.666, places=3)

    def test_load_estimate_constants(self):
        self.assertAlmostEqual(P_AUX_WATTS, 3.0, places=3)
        self.assertAlmostEqual(ETA_BUCK_HBRIDGE, 0.92, places=3)


class ReachabilityTests(unittest.TestCase):
    """The 120 V target has to be inside the modulator's authority."""

    def test_120_v_target_is_reachable(self):
        m_target = OUTPUT_TARGET_RMS * math.sqrt(2.0) / (A_DROOP * VBUS_NOMINAL)
        self.assertLessEqual(m_target, PWM_M_MAX)
        # 21.75 V bus: 120 V at no load needs about 0.879 of the 0.9597
        # available, leaving ~8 % modulation headroom.
        self.assertAlmostEqual(m_target, 0.8793, places=3)

    def test_120_v_is_unreachable_on_the_old_17_v_bus(self):
        bus_17 = 17.0 / math.sqrt(2.0) * TRANSFORMER_NOLOAD_RATIO
        m_120 = 120.0 / bus_17
        self.assertGreater(m_120, PWM_M_MAX)
        # That was why the bus was raised.
        self.assertAlmostEqual(m_120, 1.1251, places=3)

    def test_120_v_at_full_load_is_reachable_on_the_nominal_bus(self):
        m = modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, VBUS_NOMINAL)
        self.assertLessEqual(m, PWM_M_MAX)
        self.assertAlmostEqual(m, 0.9139, places=3)

    def test_120_v_at_full_load_is_reachable_at_the_bottom_of_the_bus_band(self):
        # The worst case: the bottom of the tolerance band with the
        # design's full load, where the droop is largest.
        m = modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, BUS_BAND_MIN)
        self.assertLessEqual(m, PWM_M_MAX)
        self.assertAlmostEqual(m, 0.9408, places=3)
        # 1.97 % of modulation left — the whole margin at that corner.
        self.assertAlmostEqual((PWM_M_MAX - m) / PWM_M_MAX, 0.0197, places=3)

    def test_120_v_at_full_load_is_reachable_at_the_top_of_the_bus_band(self):
        m = modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, BUS_BAND_MAX)
        self.assertLessEqual(m, PWM_M_MAX)
        self.assertAlmostEqual(m, 0.8880, places=3)

    def test_the_margin_at_every_load_and_corner(self):
        # The brief's table: the modulation for 120 V at 0, 50 and 100 W at
        # both corners of the bus band, and the margin to the clamp.
        expected = {
            (0.0, BUS_BAND_MIN): (0.9052, 0.0546),
            (50.0, BUS_BAND_MIN): (0.9230, 0.0367),
            (100.0, BUS_BAND_MIN): (0.9408, 0.0189),
            (0.0, BUS_BAND_MAX): (0.8543, 0.1054),
            (50.0, BUS_BAND_MAX): (0.8712, 0.0886),
            (100.0, BUS_BAND_MAX): (0.8880, 0.0718),
        }
        for (p_out, vbus), (m_want, margin_want) in expected.items():
            with self.subTest(p_out=p_out, vbus=vbus):
                m = modulation_for_outlet(OUTPUT_TARGET_RMS, p_out, vbus)
                self.assertAlmostEqual(m, m_want, places=3)
                self.assertAlmostEqual(PWM_M_MAX - m, margin_want, places=3)
                self.assertGreater(PWM_M_MAX - m, 0.0)

    def test_the_ceiling_cannot_reach_120_v_at_the_bottom_of_the_run_window(self):
        # The run window is wider than the modulation budget allows: below
        # 20.71 V the clamp cannot hold 120 V at 100 W, which is what the
        # "output out of reach" fault is for.
        vbus = VBUS_NOMINAL - VBUS_RUN_WINDOW
        m = modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, vbus)
        self.assertGreater(m, PWM_M_MAX)
        self.assertAlmostEqual(m, 0.9817, places=3)

    def test_droop_constant_fits_inside_the_modulation_budget(self):
        # With the catalogue constants, 120 V at 100 W at the bottom of the
        # bus band needs m = 0.9408 against the 0.9597 clamp: 1.97 % of
        # modulation to spare, which is the whole margin on this design.
        m = modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, BUS_BAND_MIN)
        self.assertLessEqual(m, PWM_M_MAX)
        self.assertAlmostEqual((PWM_M_MAX - m) / PWM_M_MAX, 0.0197, places=3)

    def test_a_nine_percent_regulating_transformer_just_fits(self):
        # 9 % regulation still reaches 120 V at the bottom of the bus
        # band, but with only 0.68 % of modulation left.
        m = fitted_modulation_at_band_min(0.09)
        self.assertLessEqual(m, PWM_M_MAX)
        self.assertAlmostEqual(m, 0.9532, places=3)
        self.assertAlmostEqual((PWM_M_MAX - m) / PWM_M_MAX, 0.0068, places=3)

    def test_a_ten_percent_regulating_transformer_does_not_fit(self):
        # 10 % regulation needs more modulation than the clamp allows at
        # the bottom of the bus band: 120 V at 100 W is out of reach, and
        # the firmware has to say so.
        m = fitted_modulation_at_band_min(0.10)
        self.assertGreater(m, PWM_M_MAX)
        self.assertAlmostEqual(m, 0.9657, places=3)

    def test_how_much_worse_the_regulation_may_be_before_the_clamp_binds(self):
        # The clamp binds between 9.5 % and 9.6 % of actual regulation, at
        # 100 W and the bottom of the bus band.  The catalogue figure is
        # 8 % TYP with no published maximum, so the regulation of the
        # fitted unit is what decides whether 120 V at 100 W is reachable.
        self.assertLessEqual(fitted_modulation_at_band_min(0.095), PWM_M_MAX)
        self.assertGreater(fitted_modulation_at_band_min(0.096), PWM_M_MAX)

    def test_receptacle_clamp_holds_the_no_load_outlet(self):
        # The clamp limits the drive so the ESTIMATED outlet cannot exceed
        # the 125 V receptacle rating.
        m_clamp = OUTLET_V_MAX * math.sqrt(2.0) / (A_DROOP * VBUS_NOMINAL)
        self.assertLessEqual(m_clamp, PWM_M_MAX)
        outlet = droop_estimate(bridge_primary_rms(m_clamp), 0.0)
        self.assertLessEqual(outlet, OUTLET_V_MAX)
        self.assertAlmostEqual(outlet, 125.0, places=6)

    def test_the_pwm_clamp_alone_would_exceed_the_receptacle_rating(self):
        # Without the OUTLET_V_MAX ceiling the no-load outlet at the PWM
        # clamp would be about 131 V, so the ceiling is not decorative.
        no_load_ceiling = expected_bridge_rms(PWM_M_MAX)
        self.assertGreater(no_load_ceiling, OUTLET_V_MAX)
        self.assertAlmostEqual(no_load_ceiling, 130.97, places=1)

    def test_clamp_allows_more_drive_at_load(self):
        # The ceiling is load-aware: at load the estimate falls, so more
        # drive is allowed.  A no-load bound applied at every load would
        # cap the drive below what is needed to hold 120 V at load.
        m_no_load = OUTLET_V_MAX * math.sqrt(2.0) / (A_DROOP * VBUS_NOMINAL)
        m_loaded = outlet_drive_limit(FULL_LOAD_W, VBUS_NOMINAL)
        self.assertGreater(m_loaded, m_no_load)
        self.assertLessEqual(
            modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, VBUS_NOMINAL),
            m_loaded,
        )


class ModulationCeilingTests(unittest.TestCase):
    """The reachable maximum outlet, and the 'out of reach' detection."""

    def test_the_ceiling_output_at_the_bus_corners(self):
        # V_out = k * m * Vbus / sqrt(2) at the clamp, with the droop
        # subtracted at load.
        for vbus, no_load, at_100w in (
            (BUS_BAND_MIN, 127.23, 122.61),
            (VBUS_NOMINAL, 130.97, 126.50),
            (BUS_BAND_MAX, 134.80, 130.46),
        ):
            with self.subTest(vbus=vbus):
                drive = PWM_M_MAX * vbus / math.sqrt(2.0)
                self.assertAlmostEqual(droop_estimate(drive, 0.0), no_load, places=1)
                self.assertAlmostEqual(droop_estimate(drive, FULL_LOAD_W), at_100w, places=1)

    def test_the_design_point_does_not_trip_the_out_of_reach_fault(self):
        # 120 V at 100 W at the bottom of the bus band, with the 8 %
        # catalogue constants: the regulator has 1.97 % of modulation left
        # and the estimate sits on the target, so nothing faults.
        vbus = BUS_BAND_MIN
        m = modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, vbus)
        drive = bridge_primary_rms(m, vbus)
        limit = outlet_drive_limit(FULL_LOAD_W, vbus)
        self.assertLess(m, limit)
        self.assertFalse(regulator_out_of_reach(drive, FULL_LOAD_W, m, limit))

    def test_a_ten_percent_unit_is_reported_as_out_of_reach(self):
        # The same operating point with a unit that actually regulates 10 %
        # and the constants fitted to it: the regulator saturates at the
        # clamp and the estimate stays below the target, so the fault
        # fires.
        k, z1 = transformer_model(0.10)
        vbus = BUS_BAND_MIN
        i_sec = FULL_LOAD_W / OUTPUT_TARGET_RMS
        drive_needed = (OUTPUT_TARGET_RMS + z1 * i_sec) / k
        m_needed = drive_needed * math.sqrt(2.0) / vbus
        self.assertGreater(m_needed, PWM_M_MAX)
        # Saturated: the regulator can only deliver the clamp.
        drive = bridge_primary_rms(PWM_M_MAX, vbus)
        limit = outlet_drive_limit(FULL_LOAD_W, vbus, z1, k)
        self.assertAlmostEqual(limit, PWM_M_MAX, places=6)
        self.assertTrue(regulator_out_of_reach(drive, FULL_LOAD_W, PWM_M_MAX, limit, k, z1))
        # The estimate at the clamp is below the target.
        estimate = droop_estimate(drive, FULL_LOAD_W, z1, k)
        self.assertLess(estimate, OUTPUT_TARGET_RMS - OUT_OF_REACH_BAND_V)

    def test_a_nine_percent_unit_is_not_reported_as_out_of_reach(self):
        k, z1 = transformer_model(0.09)
        vbus = BUS_BAND_MIN
        i_sec = FULL_LOAD_W / OUTPUT_TARGET_RMS
        drive_needed = (OUTPUT_TARGET_RMS + z1 * i_sec) / k
        m_needed = drive_needed * math.sqrt(2.0) / vbus
        self.assertLess(m_needed, PWM_M_MAX)
        drive = bridge_primary_rms(m_needed, vbus)
        limit = outlet_drive_limit(FULL_LOAD_W, vbus, z1, k)
        # Not saturated: the regulator has 0.68 % of modulation in hand.
        self.assertLess(m_needed, limit - 1e-4)
        self.assertFalse(regulator_out_of_reach(drive, FULL_LOAD_W, m_needed, limit, k, z1))
        # And the estimate sits on the target.
        self.assertAlmostEqual(
            droop_estimate(drive, FULL_LOAD_W, z1, k), OUTPUT_TARGET_RMS, places=3
        )

    def test_sitting_at_the_receptacle_ceiling_is_not_a_fault(self):
        # At low load the binding ceiling is the 125 V receptacle limit,
        # not the PWM clamp.  Sitting there is correct behaviour.
        limit = outlet_drive_limit(0.0, VBUS_NOMINAL)
        self.assertLess(limit, PWM_M_MAX)
        drive = bridge_primary_rms(limit, VBUS_NOMINAL)
        self.assertFalse(regulator_out_of_reach(drive, 0.0, limit, limit))

    def test_the_out_of_reach_fault_is_debounced(self):
        self.assertGreaterEqual(OUT_OF_REACH_TICKS, 10)
        self.assertGreater(OUT_OF_REACH_BAND_V, 0.0)
        self.assertLess(OUT_OF_REACH_BAND_V, 1.0)


class CatalogueConstantTests(unittest.TestCase):
    """What the fixed catalogue constants deliver on a real transformer."""

    def test_the_catalogue_constants_give_120_v_on_an_eight_percent_unit(self):
        k, z1 = transformer_model(0.08)
        for p_out in (0.0, 50.0, 100.0):
            with self.subTest(p_out=p_out):
                drive = (
                    modulation_for_outlet(OUTPUT_TARGET_RMS, p_out, VBUS_NOMINAL)
                    * VBUS_NOMINAL
                    / math.sqrt(2.0)
                )
                self.assertAlmostEqual(
                    physical_outlet(k, z1, drive, p_out),
                    OUTPUT_TARGET_RMS,
                    places=2,
                )

    def test_a_nine_percent_unit_reads_low_with_the_catalogue_constants(self):
        # With the 8 % constants, a unit that actually regulates 9 % is
        # driven too lightly and the outlet reads low.
        k, z1 = transformer_model(0.09)
        drive = (
            modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, VBUS_NOMINAL)
            * VBUS_NOMINAL
            / math.sqrt(2.0)
        )
        outlet = physical_outlet(k, z1, drive, FULL_LOAD_W)
        self.assertLess(outlet, 119.0)
        self.assertAlmostEqual(outlet, 118.36, places=1)

    def test_a_ten_percent_unit_reads_low_with_the_catalogue_constants(self):
        k, z1 = transformer_model(0.10)
        drive = (
            modulation_for_outlet(OUTPUT_TARGET_RMS, FULL_LOAD_W, VBUS_NOMINAL)
            * VBUS_NOMINAL
            / math.sqrt(2.0)
        )
        outlet = physical_outlet(k, z1, drive, FULL_LOAD_W)
        self.assertLess(outlet, 118.0)
        self.assertAlmostEqual(outlet, 116.76, places=1)

    def test_the_catalogue_constants_hold_the_outlet_within_three_percent(self):
        # The contract the firmware ships against: the fixed constants keep
        # the real outlet within 3 % of 120.0 V RMS across the load range
        # for a transformer regulating 6-10 %, which brackets the 8 % TYP
        # catalogue value.  No unit has to be fitted or calibrated.
        for regulation in (0.06, 0.07, 0.08, 0.09, 0.10):
            k, z1 = transformer_model(regulation)
            for p_out in (0.0, 25.0, 50.0, 75.0, 100.0):
                with self.subTest(regulation=regulation, p_out=p_out):
                    drive = (
                        modulation_for_outlet(OUTPUT_TARGET_RMS, p_out, VBUS_NOMINAL)
                        * VBUS_NOMINAL
                        / math.sqrt(2.0)
                    )
                    outlet = physical_outlet(k, z1, drive, p_out)
                    self.assertAlmostEqual(
                        outlet,
                        OUTPUT_TARGET_RMS,
                        delta=0.03 * OUTPUT_TARGET_RMS,
                    )

    def test_the_eight_percent_catalogue_unit_lands_on_the_target(self):
        k, z1 = transformer_model(0.08)
        for p_out in (0.0, 25.0, 50.0, 75.0, 100.0):
            with self.subTest(p_out=p_out):
                drive = (
                    modulation_for_outlet(OUTPUT_TARGET_RMS, p_out, VBUS_NOMINAL)
                    * VBUS_NOMINAL
                    / math.sqrt(2.0)
                )
                self.assertAlmostEqual(
                    physical_outlet(k, z1, drive, p_out),
                    OUTPUT_TARGET_RMS,
                    places=2,
                )

    def test_the_old_constants_would_have_run_the_outlet_seven_percent_low(self):
        # On the record: A = 115/12 and B = 6.6 Ω asked for too little
        # drive and the outlet sat at about 111.5 V.
        k, z1 = transformer_model(0.08)
        old_a = 115.0 / 12.0
        old_b = 6.6
        i_sec = FULL_LOAD_W / OUTPUT_TARGET_RMS
        drive = (OUTPUT_TARGET_RMS + old_b * i_sec) / old_a
        outlet = physical_outlet(k, z1, drive, FULL_LOAD_W)
        self.assertAlmostEqual(outlet, 111.48, places=1)
        self.assertLess(outlet, 112.0)


class DroopCompensationTests(unittest.TestCase):
    """The closed-form outlet estimate."""

    CASES = [
        # (drive winding RMS in volts, estimated output power in watts)
        (13.5235, 0.0),
        (13.7895, 50.0),
        (14.0556, 100.0),
        (11.0000, 25.0),
        (15.0000, 75.0),
    ]

    def test_closed_form_solves_the_estimate_quadratic(self):
        for v_drive, p_out in self.CASES:
            with self.subTest(v_drive=v_drive, p_out=p_out):
                x = A_DROOP * v_drive
                y = B_DROOP * p_out
                v = droop_estimate(v_drive, p_out)
                if y <= 0.0:
                    continue
                # V^2 - x*V + y = 0
                self.assertAlmostEqual(v * v - x * v + y, 0.0, places=4)

    def test_estimate_reproduces_the_droop_model(self):
        for v_drive, p_out in self.CASES:
            with self.subTest(v_drive=v_drive, p_out=p_out):
                if p_out <= 0.0:
                    continue
                x = A_DROOP * v_drive
                v = droop_estimate(v_drive, p_out)
                i_sec = p_out / max(v, 1.0)
                # V_out = x - B_DROOP * I_sec
                self.assertAlmostEqual(v + B_DROOP * i_sec, x, places=4)

    def test_zero_load_reduces_to_the_no_load_ratio(self):
        v = droop_estimate(13.5235, 0.0)
        self.assertAlmostEqual(v, A_DROOP * 13.5235, places=6)

    def test_estimate_is_below_the_naive_inference_under_load(self):
        # The naive inference (drive * no-load ratio) OVER-estimates the
        # outlet at load; the compensation must bring it down.
        for v_drive, p_out in self.CASES:
            if p_out <= 0.0:
                continue
            with self.subTest(v_drive=v_drive, p_out=p_out):
                v = droop_estimate(v_drive, p_out)
                self.assertLess(v, A_DROOP * v_drive)

    def test_heavier_load_lowers_the_estimate(self):
        v_drive = 14.0556
        previous = droop_estimate(v_drive, 0.0)
        for p_out in (10.0, 25.0, 50.0, 75.0, 100.0):
            with self.subTest(p_out=p_out):
                v = droop_estimate(v_drive, p_out)
                self.assertLess(v, previous)
                previous = v

    def test_estimate_sits_on_the_target_at_the_regulated_drive(self):
        # The regulator drives the estimate to 120 V: for every load the
        # drive it needs must make the estimate come out at the target.
        for p_out in (0.0, 25.0, 50.0, 75.0, 100.0):
            with self.subTest(p_out=p_out):
                v_drive = (OUTPUT_TARGET_RMS + B_DROOP * p_out / OUTPUT_TARGET_RMS) / A_DROOP
                self.assertAlmostEqual(droop_estimate(v_drive, p_out), OUTPUT_TARGET_RMS, places=4)

    def test_estimate_is_the_physical_root(self):
        # The larger root is the physical one: the smaller root collapses
        # to zero at no load and to the droop voltage under load, which
        # would make the regulator slam the modulation into the clamp.
        v_drive, p_out = 14.0556, 100.0
        x = A_DROOP * v_drive
        y = B_DROOP * p_out
        root = math.sqrt(x * x - 4.0 * y)
        large = 0.5 * (x + root)
        small = 0.5 * (x - root)
        self.assertAlmostEqual(droop_estimate(v_drive, p_out), large, places=6)
        self.assertGreater(large, small)
        # The small root is the degenerate branch: zero at no load, and a
        # few volts at 100 W.  Only the large root reproduces the model.
        self.assertEqual(0.5 * (x - x), 0.0)
        self.assertLess(small, 10.0)
        self.assertAlmostEqual(large + B_DROOP * (p_out / large), x, places=4)

    def test_discriminant_guard_treats_an_unsupportable_load_as_zero(self):
        # A drive that cannot support the load has no real root: the
        # firmware treats the load as zero and raises the "cannot support
        # the load" condition.
        v_drive, p_out = 5.0, 200.0
        x = A_DROOP * v_drive
        y = B_DROOP * p_out
        self.assertLess(x * x, 4.0 * y)
        self.assertEqual(droop_estimate(v_drive, p_out), x)

    def test_the_estimate_never_leaves_the_receptacle_rating(self):
        # Whatever the load, the drive the firmware allows keeps the
        # estimated outlet at or below 125 V.
        for p_out in (0.0, 25.0, 50.0, 75.0, 100.0, 200.0):
            with self.subTest(p_out=p_out):
                m = outlet_drive_limit(p_out)
                v = droop_estimate(bridge_primary_rms(m), p_out)
                self.assertLessEqual(v, OUTLET_V_MAX + 1e-6)


class BusWindowTests(unittest.TestCase):
    def test_windows_stay_below_the_ain1_clip(self):
        # A clipped reading must never satisfy busValidForStart/Run.
        self.assertGreater(AIN1_FULL_SCALE, VBUS_NOMINAL + VBUS_RUN_WINDOW)
        self.assertGreater(AIN1_FULL_SCALE, VBUS_NOMINAL + VBUS_START_WINDOW)
        self.assertAlmostEqual(AIN1_FULL_SCALE, 25.74, places=2)

    def test_lower_window_is_positive(self):
        self.assertGreater(VBUS_NOMINAL - VBUS_RUN_WINDOW, 0.0)

    def test_the_input_rail_divider_is_unchanged(self):
        self.assertAlmostEqual(DIV_AIN0, _divider(36.5, 10.0), places=9)
        self.assertGreater(3.3 / DIV_AIN0, 14.0)

    def test_the_nominal_bus_sits_inside_the_adc_range(self):
        self.assertLess(VBUS_NOMINAL * DIV_AIN1, 3.3)
        self.assertAlmostEqual(VBUS_NOMINAL * DIV_AIN1, 2.788, places=2)


class OutputMissingTests(unittest.TestCase):
    def test_old_trip_fired_during_the_ramp(self):
        # The old check was mRamp > 0.05 && outputRms < 11.5 V.  At m=0.05
        # the fundamental is 6.83 V on the 21.75 V bus, so the check tripped
        # as soon as the ramp crossed 5 %.
        self.assertLess(expected_bridge_rms(0.05), 11.5)
        self.assertAlmostEqual(expected_bridge_rms(0.05), 6.82, places=2)

    def test_new_check_ignores_the_low_ramp(self):
        # The new check only runs when the expected output is above 15 V.
        self.assertLess(expected_bridge_rms(0.10), 15.0)
        self.assertGreater(expected_bridge_rms(0.11), 15.0)

    def test_new_check_passes_at_the_old_trip_point(self):
        expected = expected_bridge_rms(0.15)
        threshold = OUT_MISSING_FRACTION * expected
        self.assertGreater(expected, 15.0)
        self.assertAlmostEqual(threshold, 12.28, places=2)
        # A healthy bridge at m=0.15 is well above its own threshold.
        self.assertGreater(expected, threshold)

    def test_new_check_passes_at_full_load(self):
        # At the clamp the bridge makes 131.0 V on the outlet scale; the
        # loaded outlet is about 126.3 V, well above the 60 % threshold.
        expected = expected_bridge_rms(PWM_M_MAX)
        full_load = expected - B_DROOP * (FULL_LOAD_W / OUTPUT_TARGET_RMS)
        self.assertGreater(full_load, OUT_MISSING_FRACTION * expected)

    def test_dead_bridge_trips(self):
        expected = expected_bridge_rms(PWM_M_MAX)
        self.assertLess(0.0, OUT_MISSING_FRACTION * expected)
        # The sketch's debounce literal must be 5: five 100 Hz ticks are
        # 50 ms of missing bridge before the latch.
        block = SOURCE[SOURCE.index("inferredOutletRms < OUT_MISSING_FRACTION") :]
        block = block[: block.index("}", block.index("outMissingCount"))]
        self.assertIn("if (++outMissingCount >= 5)", block)
        self.assertIn("expected > 15.0f", SOURCE)

    def test_overvoltage_is_supervisory(self):
        # The estimate is held at or below 125 V by the drive ceiling, so
        # the 126 V trip is the backstop above it.
        ov = OUTPUT_TARGET_RMS * OUTPUT_OV_FACTOR
        self.assertAlmostEqual(ov, 126.0, places=1)
        self.assertGreater(ov, OUTLET_V_MAX)
        self.assertLess(ov, expected_bridge_rms(PWM_M_MAX))


class DividerFilterTests(unittest.TestCase):
    """C29/C30 sit across the 10 kΩ bottom resistor of each divider."""

    CORNER_HZ = 1.0 / (2.0 * math.pi * 10000.0 * 47e-9)

    def test_the_corner_is_339_hz_not_75_hz(self):
        self.assertAlmostEqual(self.CORNER_HZ, 338.6, places=1)
        self.assertGreater(self.CORNER_HZ, 300.0)
        self.assertLess(self.CORNER_HZ, 400.0)

    def test_the_gain_error_at_60_hz_is_1_5_percent(self):
        gain = 1.0 / math.sqrt(1.0 + (60.0 / self.CORNER_HZ) ** 2)
        self.assertAlmostEqual(gain, 0.9847, places=3)
        self.assertLess(gain, 0.99)

    def test_the_lag_at_60_hz_is_ten_degrees(self):
        phase = math.degrees(math.atan2(60.0, self.CORNER_HZ))
        self.assertAlmostEqual(phase, 10.0, places=1)

    def test_the_error_is_not_corrected_in_firmware(self):
        # A correction would have to appear in the divider constants; they
        # are the plain 68k/10k ratios, and the error is absorbed by the
        # per-unit A_DROOP fit instead.
        self.assertAlmostEqual(DIV_AIN2, _divider(68.0, 10.0), places=9)
        self.assertAlmostEqual(DIV_AIN3, _divider(68.0, 10.0), places=9)

    def test_the_error_is_recorded_in_the_sketch(self):
        self.assertIn("339 Hz", SOURCE)
        self.assertIn("1.5 %", SOURCE)
        self.assertIn("C29 and C30 (47 nF)", SOURCE)


class DcOffsetTests(unittest.TestCase):
    def test_the_threshold_and_debounce(self):
        # The flux knee is 11-13 ms for a 1 V offset at full modulation.  A
        # 0.5 V threshold with two consecutive estimates is the fastest
        # windowed 60 Hz estimate the transformer's flux budget allows; see
        # tests/test_v19_protection_latency.py for the latency model.
        self.assertAlmostEqual(DC_TRIP_V, 0.5, places=6)
        self.assertEqual(DC_TRIP_SAMPLES, 2)

    def test_the_trip_is_evaluated_on_every_rms_fit(self):
        # Not on the 100 Hz control tick: that alone would add 10 ms, which
        # is most of the saturation window.
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ====")]
        self.assertIn("if (!dcSuspect && fabsf(primaryDc) > DC_TRIP_V)", block)
        self.assertIn('setFault("DC offset")', block)
        control = SOURCE[SOURCE.index("static void runControl(") :]
        control = control[: control.index("void powerBegin()")]
        self.assertNotIn('setFault("DC offset")', control)

    def test_the_estimate_is_a_fundamental_fit_not_a_window_mean(self):
        # The old estimate differenced two HELD midpoint values, so the 60 Hz
        # fundamental aliased against the sweep.  The fit estimates each
        # channel's DC and fundamental separately from the carrier phase and
        # differences the intercepts.
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ====")]
        self.assertIn("fitMidRing(midRingA", block)
        self.assertIn("fitMidRing(midRingB", block)
        self.assertIn("primaryDc = fa.dc - fb.dc;", block)
        self.assertNotIn("vMidA - vMidB", block)

    def test_the_fit_needs_enough_samples_and_phase_span(self):
        # A three-parameter fit needs at least four samples, and a set that
        # spans too little phase is rejected rather than returning a wild DC.
        self.assertGreaterEqual(RMS_MIN_SAMPLES, 4)
        self.assertGreater(RMS_MIN_SPAN_US, 10000)
        self.assertLess(RMS_MIN_SPAN_US, RMS_WINDOW_US)
        block = SOURCE[SOURCE.index("static FundamentalFit fitMidRing") :]
        block = block[: block.index("static void updateRmsAndDc()")]
        self.assertIn("used < RMS_MIN_SAMPLES", block)
        self.assertIn("(uint32_t)(newestUs - oldestUs) < RMS_MIN_SPAN_US", block)
        self.assertIn("fabsf(det) < 1e-3f * scale", block)

    def test_a_fit_older_than_the_stale_limit_is_dropped(self):
        # rmsValid must not outlive its data: a chip that stops converting
        # without NACKing would otherwise leave the last fit valid forever.
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ====")]
        self.assertIn("(uint32_t)(micros() - midLastSampleUs) > RMS_STALE_US", block)
        self.assertIn("rmsValid = false;", block)
        self.assertLess(RMS_STALE_US, RMS_MIN_SPAN_US)

    def test_the_trip_is_armed_only_once_the_fit_is_valid(self):
        # A part-full ring cannot produce a fit at all; the trip lives after
        # the `if (!fa.ok || !fb.ok)` guard.
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ====")]
        guard = block.index("if (!fa.ok || !fb.ok)")
        trip = block.index("if (!dcSuspect && fabsf(primaryDc) > DC_TRIP_V)")
        self.assertGreater(trip, guard)
        self.assertIn("rmsValid = true;", block)

    def test_silent_death_is_named_not_misdiagnosed(self):
        # A chip whose ADC has stopped (but whose I2C still answers) returns
        # the same 12-bit code forever.  Three identical midpoint codes in a
        # row while the bridge is switching means the data is dead: fault
        # "sensor stale" at sample time instead of letting the stale samples
        # trip the DC-offset check with a misleading reason.
        block = SOURCE[SOURCE.index("float volts = adsCodeToVolts(code);") :]
        block = block[: block.index("/* ====")]
        self.assertIn("static uint16_t adsRepCode[2] = {0xFFFF, 0xFFFF};", SOURCE)
        self.assertIn("if (++adsRepCount[m] >= 3)", block)
        self.assertIn('adsFail("sensor stale");', block)
        probe = SOURCE[SOURCE.index("static bool adsProbe(") :]
        probe = probe[: probe.index("static float adsCodeToVolts")]
        self.assertIn("adsRepCode[0] = adsRepCode[1] = 0xFFFF;", probe)

    def test_the_trip_stands_down_on_suspect_data(self):
        # Frozen samples stamped with fresh phases look like an offset to a
        # fit.  While a midpoint code repeats, the trip must not judge: the
        # silent-death detector owns that case and names it accurately.
        block = SOURCE[SOURCE.index("static void updateRmsAndDc()") :]
        block = block[: block.index("/* ====")]
        self.assertIn("bool dcSuspect = (adsRepCount[0] >= 1) || (adsRepCount[1] >= 1);", block)
        self.assertIn("if (!dcSuspect && fabsf(primaryDc) > DC_TRIP_V)", block)

    def test_the_measurement_window_is_cleared_between_runs(self):
        # Otherwise the tail of the previous run, and the filter's settling
        # transient, would look like a DC offset.
        for func in ("static void stopBridge()", "static void startBridge()"):
            block = SOURCE[SOURCE.index(func) :]
            block = block[: block.index("}", block.index(func) + len(func))]
            self.assertIn("resetMeasurementWindow();", block)

    def test_the_sketch_comment_does_not_claim_the_stale_figure(self):
        self.assertNotIn("~90 ms to saturate", SOURCE)
        self.assertNotIn("primary DC offset; ~90 ms", SOURCE)


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

    def test_the_regulator_can_reach_the_clamp(self):
        # At the full slew the integrator walks from the soft-start value
        # to the clamp in about 0.66 s, so the "out of reach" condition is
        # reachable rather than theoretical.  The debounce measures the
        # time AT the ceiling, not the walk to it.
        rate = AMP_SLEW_MAX / (CTRL_INTERVAL_MS / 1000.0)
        seconds = (PWM_M_MAX - M_START) / rate
        self.assertLess(seconds, 2.0)
        self.assertGreater(seconds, 0.1)


class LoadLimitTests(unittest.TestCase):
    """The transformer's VA limit, reported as 'load too heavy'."""

    def test_the_limit_is_above_the_product_rating(self):
        self.assertGreater(LOAD_LIMIT_W, 100.0)
        self.assertLess(LOAD_LIMIT_W, 160.0)

    def test_the_limit_is_debounced(self):
        self.assertGreaterEqual(LOAD_HEAVY_TICKS, 10)

    def test_the_limit_is_a_share_of_the_transformer_rating(self):
        # The VPT24-6670 is 160 VA.  The limit is on the ESTIMATED output
        # power, which runs about 10 % high, so 150 W of estimate is about
        # 135 W at the outlet: 85 % of the VA rating.
        self.assertAlmostEqual(LOAD_LIMIT_W / 160.0, 0.9375, places=3)
        true_load = (LOAD_LIMIT_W / ETA_BUCK_HBRIDGE + P_AUX_WATTS) * 0.92 * 0.89
        self.assertAlmostEqual(true_load, 135.9, places=0)
        self.assertLess(true_load / 160.0, 0.9)


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
        # Every failure path inside adsService goes through adsFail(), which
        # drops the presence and clock flags and forces a fresh sweep.  One
        # path is deliberate: the four separate resets used to drift apart.
        fail = SOURCE[SOURCE.index("static void adsFail(") :]
        fail = fail[: fail.index("static void adsService()")]
        for name in (
            "adsPresent = false;",
            "adsClockOk = false;",
            "adsPhase = ADS_IDLE;",
            "adsSwept = false;",
            "adsSeenMask = 0;",
        ):
            with self.subTest(name=name):
                self.assertIn(name, fail)
        service = SOURCE[SOURCE.index("static void adsService()") :]
        service = service[: service.index("// Freshness of one ADS channel")]
        self.assertGreaterEqual(service.count("adsFail("), 4)
        self.assertNotIn("adsPresent = false;", service)
        self.assertNotIn("adsSwept = false;", service)
        # powerClearFault() re-arms the probe on its own, since it is called
        # while disarmed and must not rely on a failure path.
        clear = SOURCE[SOURCE.index("void powerClearFault() {") :]
        clear = clear[: clear.index("bool powerArmed() {")]
        self.assertIn("adsPresent = false;", clear)
        self.assertIn("adsClockOk = false;", clear)
        self.assertIn("adsSwept = false;", clear)

    def test_watchdog_covers_the_boot_sweep(self):
        self.assertIn("watchdog_enable(500, true);", SOURCE)
        # Parse the sweep from setup(): NUM_LEDS iterations of delay(N) plus
        # the closing delay.  Hardcoding 5*18+70 would go stale silently.
        setup = SOURCE[SOURCE.index("void setup() {") :]
        setup = setup[: setup.index("/* ====")]
        import re as _re
        leds = int(_re.search(r"NUM_LEDS\s+(\d+)", SOURCE).group(1))
        delays = [int(v) for v in _re.findall(r"delay\((\d+)\);\s*\n\s*watchdog_update", setup)]
        self.assertGreaterEqual(len(delays), 2)
        self.assertLess(leds * delays[0] + delays[-1], 500)
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
        self.assertRegex(SOURCE, r"#define\s+OUTPUT_TARGET_RMS\s+120\.0f")
        self.assertRegex(SOURCE, r"#define\s+OUT_MISSING_FRACTION\s+0\.60f")
        self.assertRegex(SOURCE, r"#define\s+DC_TRIP_V\s+0\.5f")
        self.assertRegex(SOURCE, r"#define\s+DC_TRIP_SAMPLES\s+2")
        self.assertRegex(SOURCE, r"#define\s+AMP_SLEW_MAX\s+0\.010f")
        self.assertRegex(SOURCE, r"#define\s+VBUS_START_WINDOW\s+0\.90f")
        self.assertRegex(SOURCE, r"#define\s+VBUS_RUN_WINDOW\s+1\.50f")
        self.assertRegex(SOURCE, r"#define\s+SINE_TABLE_SIZE\s+64")
        self.assertRegex(SOURCE, r"#define\s+SINE_PHASE_INC\s+12884902UL")
        self.assertRegex(SOURCE, r"#define\s+RMS_WINDOW_US\s+16667UL")
        self.assertIn('"bridge output missing"', SOURCE)

    def test_the_build_is_pinned_to_the_150_mhz_clock(self):
        # The PWM and sine constants produce 60 Hz only at 150 MHz.  A board
        # menu that changes the CPU frequency must fail the build, not shift
        # the output frequency.
        self.assertIn("static_assert(F_CPU == 150000000UL", SOURCE)

    def test_new_topology_constants(self):
        self.assertRegex(SOURCE, r"#define\s+VBUS_NOMINAL\s+21\.75f")
        self.assertRegex(SOURCE, r"#define\s+OUTPUT_OV_FACTOR\s+1\.05f")
        self.assertRegex(SOURCE, r"#define\s+OUTLET_V_MAX\s+125\.0f")
        self.assertRegex(SOURCE, r"#define\s+B_DROOP\s+5\.666f")
        self.assertRegex(SOURCE, r"#define\s+P_AUX_WATTS\s+3\.0f")
        self.assertRegex(SOURCE, r"#define\s+ETA_BUCK_HBRIDGE\s+0\.92f")
        self.assertRegex(SOURCE, r"#define\s+TRANSFORMER_NOLOAD_RATIO\s+\(115\.0f\s*/\s*12\.96f\)")
        self.assertRegex(SOURCE, r"#define\s+A_DROOP\s+TRANSFORMER_NOLOAD_RATIO")
        for name in ("DIV_AIN1", "DIV_AIN2", "DIV_AIN3"):
            self.assertRegex(
                SOURCE,
                rf"#define\s+{name}\s+\(10\.0f\s*/\s*\(68\.0f\s*\+\s*10\.0f\)\)",
            )

    def test_new_fault_strings(self):
        self.assertIn('"load too heavy"', SOURCE)
        self.assertIn('"output overvoltage"', SOURCE)
        self.assertIn('"output out of reach"', SOURCE)

    def test_droop_estimate_is_implemented_in_closed_form(self):
        self.assertIn("float x = A_DROOP * drivePrimaryRms();", SOURCE)
        self.assertIn("float y = B_DROOP * pOut;", SOURCE)
        self.assertIn("0.5f * (x + sqrtf(x * x - 4.0f * y))", SOURCE)
        self.assertIn("x * x < 4.0f * y", SOURCE)
        self.assertIn("float iSec = pOut / ((v > 1.0f) ? v : 1.0f);", SOURCE)

    def test_the_regulator_works_on_the_outlet_estimate(self):
        self.assertIn("float error = OUTPUT_TARGET_RMS - outletEstimate();", SOURCE)
        self.assertIn("if (outletEstimate() > OUTPUT_TARGET_RMS * OUTPUT_OV_FACTOR)", SOURCE)
        # The drive ceiling is applied to the commanded modulation.
        self.assertIn("mTarget = clampf(mTarget, 0.0f, driveLimit);", SOURCE)

    def test_the_out_of_reach_fault_is_checked_before_the_load_fault(self):
        # Otherwise the load fault would mask it.
        out_of_reach = SOURCE.index('setFault("output out of reach")')
        load_too_heavy = SOURCE.index('setFault("load too heavy")')
        self.assertLess(out_of_reach, load_too_heavy)

    def test_the_out_of_reach_counter_resets_when_not_saturated(self):
        # The debounce measures the time AT the ceiling, not the walk to
        # it: the counter is cleared on every tick where the regulator is
        # not pinned at the clamp.
        block = SOURCE[SOURCE.index("bool clampBound") :]
        block = block[: block.index("// ---- LOAD TOO HEAVY")]
        self.assertIn("if (++outOfReachCount >= OUT_OF_REACH_TICKS)", block)
        self.assertIn("outOfReachCount = 0;", block)

    def test_the_report_state_is_cleared_when_the_bridge_stops(self):
        block = SOURCE[SOURCE.index("static void stopBridge()") :]
        block = block[: block.index("static void enablePwmSlices()")]
        for name in ("outOfReachCount = 0;", "loadHeavyCount = 0;"):
            with self.subTest(name=name):
                self.assertIn(name, block)
        for name in ("mReqNow", "driveLimitNow"):
            with self.subTest(name=name):
                self.assertIn(f"{name} = 0.0f;", block)

    def test_both_load_faults_are_inside_the_running_branch(self):
        # The regulator block only runs once the bridge is running, so
        # neither fault can fire during the soft-start ramp.
        gate = SOURCE.index("if (pState == POWER_RUNNING && rmsValid) {")
        self.assertGreater(SOURCE.index('setFault("output out of reach")'), gate)
        self.assertGreater(SOURCE.index('setFault("load too heavy")'), gate)

    def test_the_discriminant_guard_is_not_a_fault_trigger(self):
        # The raw guard would trip on every load above about 74 W at the
        # end of the soft-start ramp, so it may only be reported.
        block = SOURCE[SOURCE.index("static void updateOutletEstimate()") :]
        block = block[: block.index("static float outletDriveLimit")]
        self.assertNotIn("setFault", block)

    def test_the_raw_discriminant_would_trip_at_the_end_of_the_ramp(self):
        # The reason the guard above must not be a fault trigger.
        x = A_DROOP * bridge_primary_rms(M_START)
        y = B_DROOP * FULL_LOAD_W
        self.assertLess(x * x, 4.0 * y)

    def test_the_serial_report_carries_the_modulation_margin(self):
        for field in ('" MMAX="', '" MREQ="', '" DLIM="', '" SAT="', '" CLAMP="'):
            with self.subTest(field=field):
                self.assertIn(field, SOURCE)

    def test_the_inferred_bridge_check_uses_the_no_load_ratio(self):
        # The same no-load ratio as the droop model, and it must be the
        # no-load (open-circuit) value, not the full-load one.
        self.assertIn("inferredOutletRms = primaryRms * TRANSFORMER_NOLOAD_RATIO;", SOURCE)
        self.assertIn(
            "float expected = modulation * busForCheck * TRANSFORMER_NOLOAD_RATIO / SQRT2;",
            SOURCE,
        )

    def test_no_stale_topology_defines(self):
        defines = "\n".join(re.findall(r"^#define\s+.*$", SOURCE, flags=re.M))
        for stale in ("19.44", "17.0f", "45.3f", "43.2f", "1.15f", "0.40f", "20.8f", "6.6f"):
            with self.subTest(stale=stale):
                self.assertNotIn(stale, defines)

    def test_no_stale_topology_prose(self):
        for stale in ("17 V bus", "105 V", "19.44 V", "0.4 V window", "20.8 V"):
            with self.subTest(stale=stale):
                self.assertNotIn(stale, SOURCE)

    def test_no_stale_transformer_ratio(self):
        self.assertNotIn("TRANSFORMER_RATIO ", SOURCE)
        self.assertNotIn("#define TRANSFORMER_RATIO", SOURCE)

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


class HeaderTopologyTests(unittest.TestCase):
    """The header comment block has to describe the fitted topology."""

    def test_the_header_names_the_final_topology(self):
        for token in ("21.75 V bus", "VPT24-6670", "120.0 V RMS regulated outlet"):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE)

    def test_the_header_says_the_outlet_is_an_estimate(self):
        self.assertIn("regulated by ESTIMATE", SOURCE)
        self.assertIn("isolated output sense", SOURCE)
        self.assertIn("droop-compensated feed forward", SOURCE)

    def test_the_header_names_the_boost_feedback_divider(self):
        self.assertIn("64.9 kΩ", SOURCE)

    def test_the_constants_are_the_catalogue_values_with_no_per_unit_fit(self):
        # Nobody fits A_DROOP and B_DROOP to their own unit.  The sketch
        # uses the transformer's catalogue values as they come.
        self.assertIn("Both constants are the transformer's CATALOGUE values", SOURCE)
        self.assertNotIn("A_DROOP = V_out0 / V_drive_rms", SOURCE)
        self.assertNotIn("B_DROOP = (x1 - x2) / (I_sec1 - I_sec2)", SOURCE)

    def test_the_header_says_nothing_senses_the_outlet(self):
        for token in ("NO voltage and NO current measurement after the", "ESTIMATE"):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE)

    def test_the_sketch_states_the_three_percent_bound(self):
        self.assertIn("within 3 % of 120.0 V RMS", SOURCE)
        for token in ("6 %", "10 %", "120.00", "116.76"):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE)


class FirmwareContractTests(unittest.TestCase):
    """Behaviour the source must exhibit, checked against the source itself.

    These close the gaps the audit listed: the PWM clamp parsed from the
    sketch's own expression (not a locally recomputed constant), the sine
    table values, the regulator converging in simulation, every fault path
    parking the gates, and every arming refusal.
    """

    def test_pwm_clamp_comes_from_the_sketch_expression(self):
        match = re.search(
            r"#define\s+PWM_M_MAX\s+\(\(float\)\(PWM_TOP \+ 1 - 4 \* PWM_DT_TICKS - 2\)"
            r" / \(float\)\(PWM_TOP \+ 1\)\)",
            SOURCE,
        )
        self.assertIsNotNone(match)
        self.assertEqual(PWM_TOP, 7499)
        self.assertEqual(PWM_DT_TICKS, 75)
        self.assertAlmostEqual(PWM_M_MAX, 0.959733, places=6)

    def test_dead_time_comes_from_the_sketch_expression(self):
        # 75 ticks at 150 MHz is 500 ns on each edge of each leg; the
        # inverted-channel compare carries the 2*dt offset, rounded (not
        # truncated) so the programmed gap never falls below the claim.
        self.assertIn(
            "(uint16_t)(dA * (float)(PWM_TOP + 1) + 2.0f * (float)PWM_DT_TICKS + 0.5f);",
            SOURCE,
        )
        self.assertIn("pwm_set_counter(PWM_SLICE_A_LO, (uint16_t)PWM_DT_TICKS);", SOURCE)
        self.assertIn("pwm_set_counter(PWM_SLICE_B_LO, (uint16_t)(2 * PWM_DT_TICKS));", SOURCE)
        self.assertAlmostEqual(PWM_DT_TICKS / 150e6, 500e-9, places=12)

    def test_sine_table_values(self):
        block = SOURCE[SOURCE.index("static const float kSineTable") :]
        block = block[: block.index("};", block.index("kSineTable"))]
        values = [float(v) for v in re.findall(r"([+-]?[0-9]+\.[0-9]+)f", block)]
        self.assertEqual(len(values), 65)
        for k, v in enumerate(values):
            with self.subTest(k=k):
                self.assertAlmostEqual(v, math.sin(2.0 * math.pi * k / 64.0), places=8)
        self.assertAlmostEqual(values[0], 0.0, places=9)
        self.assertAlmostEqual(values[16], 1.0, places=8)
        self.assertAlmostEqual(values[64], values[0], places=9)

    def test_sine_phase_increment_is_60_hz_at_20_khz(self):
        self.assertRegex(SOURCE, r"#define\s+SINE_PHASE_INC\s+12884902UL")
        self.assertAlmostEqual(12884902 / 2.0**32 * 20000.0, 60.0, places=1)

    def test_the_regulator_converges_to_the_target(self):
        # The integral action on the outlet estimate: a fixed-point
        # iteration of the sketch's own update law drives mTarget to the
        # modulation the target needs at 100 W on a nominal bus.
        m = M_START
        for _ in range(600):
            drive = bridge_primary_rms(m, VBUS_NOMINAL)
            x = A_DROOP * drive
            y = B_DROOP * FULL_LOAD_W
            v = 0.5 * (x + math.sqrt(x * x - 4.0 * y)) if x * x >= 4.0 * y else x
            error = OUTPUT_TARGET_RMS - v
            step = error * AMP_KI
            step = max(-AMP_SLEW_MAX, min(AMP_SLEW_MAX, step))
            m = max(0.0, min(PWM_M_MAX, m + step))
        self.assertAlmostEqual(m, 0.914, delta=0.01)

    def test_the_regulator_cannot_wind_up_against_the_ceiling(self):
        block = SOURCE[SOURCE.index("float error = OUTPUT_TARGET_RMS - outletEstimate();") :]
        block = block[: block.index("mRamp = mTarget;")]
        self.assertIn("mTarget = clampf(mTarget, 0.0f, driveLimit);", block)
        self.assertIn("float step = error * AMP_KI;", block)
        self.assertIn("if (step > AMP_SLEW_MAX) step = AMP_SLEW_MAX;", block)

    def test_every_fault_path_parks_the_gates(self):
        # setFault() always calls stopBridge(), and stopBridge() always
        # calls parkGatesLow(), which disables the slices and drives all
        # four gate pins low -- so no fault can leave a gate switching.
        park = SOURCE[SOURCE.index("static void parkGatesLow()") :]
        park = park[: park.index("static void setFault(")]
        self.assertIn("pwm_set_mask_enabled(0);", park)
        for pin in ("GATE_A_HI_PIN", "GATE_A_LO_PIN", "GATE_B_HI_PIN", "GATE_B_LO_PIN"):
            with self.subTest(pin=pin):
                self.assertIn(f"digitalWrite({pin}, LOW);", park)
        fault = SOURCE[SOURCE.index("static void setFault(") :]
        fault = fault[: fault.index("// Input-rail hysteresis")]
        self.assertIn("stopBridge();", fault)
        stop = SOURCE[SOURCE.index("static void stopBridge()") :]
        stop = stop[: stop.index("static void enablePwmSlices()")]
        self.assertIn("parkGatesLow();", stop)
        self.assertIn("modulation = 0.0f;", stop)

    def test_every_arming_refusal(self):
        arm = SOURCE[SOURCE.index("bool powerSetArmed(bool on)") :]
        arm = arm[: arm.index("void powerSerialReport()")]
        for reason in (
            "ARM refused: fault latched",
            "ARM refused: no sensors",
            "ARM refused: I2C not at 400 kHz",
            "ARM refused: sensors not ready",
        ):
            with self.subTest(reason=reason):
                self.assertIn(reason, arm)
        self.assertGreaterEqual(arm.count("return false;"), 3)

    def test_current_rail_and_temperature_thresholds(self):
        self.assertRegex(SOURCE, r"#define\s+IIN_TRIP\s+20\.0f")
        self.assertRegex(SOURCE, r"#define\s+IIN_WARN\s+15\.0f")
        self.assertRegex(SOURCE, r"#define\s+IIN_TRIP_TICKS\s+2\b")
        self.assertRegex(SOURCE, r"#define\s+VIN_LOW\s+11\.0f")
        self.assertRegex(SOURCE, r"#define\s+VIN_HIGH\s+14\.0f")
        self.assertRegex(SOURCE, r"#define\s+VIN_HYST\s+0\.2f")
        self.assertRegex(SOURCE, r"#define\s+TEMP_TRIP_C\s+85\.0f")
        self.assertRegex(SOURCE, r"#define\s+TEMP_WARN_C\s+60\.0f")
        self.assertRegex(SOURCE, r"#define\s+TEMP_VALID_MIN_C\s+-40\.0f")
        self.assertRegex(SOURCE, r"#define\s+TEMP_VALID_MAX_C\s+150\.0f")

    def test_over_current_is_debounced_like_the_other_fast_trips(self):
        block = SOURCE[SOURCE.index("if (pArmed) {") :]
        block = block[: block.index("void powerBegin()")]
        self.assertIn("if (++iinOverCount >= IIN_TRIP_TICKS)", block)
        self.assertIn("iinOverCount = 0;", block)
        stop = SOURCE[SOURCE.index("static void stopBridge()") :]
        stop = stop[: stop.index("static void enablePwmSlices()")]
        self.assertIn("iinOverCount = 0;", stop)

    def test_rail_hysteresis_starts_wide_and_debounces(self):
        # A battery sitting exactly on the 11.0 V edge plus a millivolt of
        # noise must not instant-fault on the first powered tick: the memory
        # starts in the wide window (self-correcting on first evaluation),
        # and the fault itself is debounced like the other trips.
        decl = SOURCE[SOURCE.index("static bool inputRailWasValid") :]
        decl = decl[: decl.index("static bool inputRailValid")]
        self.assertIn("static bool inputRailWasValid = true;", decl)
        block = SOURCE[SOURCE.index("if (pArmed) {") :]
        block = block[: block.index("void powerBegin()")]
        self.assertIn("if (++railBadTicks >= RAIL_TRIP_TICKS)", block)
        self.assertIn("railBadTicks = 0;", block)
        self.assertRegex(SOURCE, r"#define\s+RAIL_TRIP_TICKS\s+3\b")
        stop = SOURCE[SOURCE.index("static void stopBridge()") :]
        stop = stop[: stop.index("static void enablePwmSlices()")]
        self.assertIn("railBadTicks = 0;", stop)

    def test_start_does_not_wipe_rail_hysteresis(self):
        # startBridge() runs only from the WAIT_BUS branch, which proves the
        # rail live on the same tick, so it must not clear the hysteresis
        # memory; stopBridge() resets it to the wide (true) side.
        start = SOURCE[SOURCE.index("static void startBridge()") :]
        start = start[: start.index("static void runControl(")]
        self.assertNotIn("inputRailWasValid =", start)
        stop = SOURCE[SOURCE.index("static void stopBridge()") :]
        stop = stop[: stop.index("static void enablePwmSlices()")]
        self.assertIn("inputRailWasValid = true;", stop)

    def test_over_faults_need_a_physical_reset(self):
        # Safety-critical: "over-" (over-current, over-temp) faults cannot be
        # cleared from the portal; only a reset clears them.
        clear = SOURCE[SOURCE.index("void powerClearFault() {") :]
        clear = clear[: clear.index("bool powerArmed() {")]
        self.assertIn('strncmp(pFaultReason, "over-", 5)', clear)
        self.assertIn("CLEAR ignored: over-fault latched", clear)
        self.assertLess(
            clear.index("pArmed = false;"), clear.index("CLEAR ignored")
        )

    def test_the_cursor_is_a_fixed_hot_pink(self):
        # Selection must never look like a fault: no entropy, no hue shift,
        # no brightness breathing on the cursor path.
        cursor = SOURCE[SOURCE.index("void renderCursor(uint8_t index) {") :]
        cursor = cursor[: cursor.index("\n}\n")]
        self.assertIn("leds[index] = COLOR_HOT_PINK;", cursor)
        for banned in ("beatsin8(", "entropy.", "CHSV(", "blendColor(", "nscale8("):
            with self.subTest(banned=banned):
                self.assertNotIn(banned, cursor)
        self.assertNotIn("minimumBrightness", cursor)

    def test_warnings_re_arm_after_a_stop(self):
        # A persistent over-current or over-temp must re-warn after a fault
        # clear or disarm, not print once and go silent: the latches live at
        # file scope and stopBridge() resets them.
        decl = SOURCE[SOURCE.index("static uint8_t iinOverCount") :]
        decl = decl[: decl.index("static float   primaryRms")]
        self.assertIn("static bool warnedCurrent = false;", decl)
        self.assertIn("static bool warnedTemp = false;", decl)
        stop = SOURCE[SOURCE.index("static void stopBridge()") :]
        stop = stop[: stop.index("static void enablePwmSlices()")]
        self.assertIn("warnedCurrent = false;", stop)
        self.assertIn("warnedTemp = false;", stop)

    def test_v19_has_no_games(self):
        # The inverter never plays: no Fireball, no Games menu, no in-game
        # state.  The 5V mode keeps fireplace/saber/rainbow (not games);
        # none of the game identifiers may appear.
        for banned in (
            "UI_MENU_GAMES",
            "UI_IN_GAME",
            "AppFireball",
            "appFireball",
            "renderGamesMenu",
            "gamesCursor",
            "COLOR_GAMES",
            "FIRE_GREEN",
            "FIRE_WHITE",
        ):
            with self.subTest(banned=banned):
                self.assertNotIn(banned, SOURCE)

    def test_dual_personality_switches_on_the_input_rail(self):
        # USB 5V alone reads ~0V on the rail: toys.  A battery (>=10V, fresh
        # sample) flips to inverter mode; below 9V (or a stale sample) it
        # flips back.  Either flip disarms and returns to idle.
        self.assertRegex(SOURCE, r"#define\s+INVERTER_ENTER_V\s+10\.0f")
        self.assertRegex(SOURCE, r"#define\s+INVERTER_EXIT_V\s+9\.0f")
        det = SOURCE[SOURCE.index("Power-mode detection") :]
        det = det[: det.index("// State transitions (armed only).")]
        self.assertIn("adsFresh(0, sensorNowUs) && vIn >= INVERTER_ENTER_V", det)
        self.assertIn("vIn < INVERTER_EXIT_V", det)
        self.assertIn("powerSetArmed(false);", det)
        self.assertIn("returnToIdle();", det)
        self.assertIn("ARM refused: no battery (5V mode)", SOURCE)
        self.assertIn('" MODE="', SOURCE)

    def test_home_menu_follows_the_power_mode(self):
        menu = SOURCE[SOURCE.index("void renderMainMenu()") :]
        menu = menu[: menu.index("/* ====")]
        self.assertIn("menuWindowBase(mainCursor, 3)", menu)
        self.assertIn("menuWindowBase(mainCursor, 5)", menu)
        self.assertIn("COLOR_PORTAL_BLUE", menu)
        self.assertIn("COLOR_PORTAL_PURPLE", menu)
        self.assertIn("COLOR_PORTAL_AMBER", menu)


class V18ParityTests(unittest.TestCase):
    """V1.9's 5V interface is V1.8's interface on five LEDs.

    Gestures, timings, tables, bars and the status display must match the
    perfect V1.8 reference.  The toy modes live on in V1.9's 5V mode; the
    inverter mode replaces them with the portal.
    """

    def test_gesture_thresholds_match_v18(self):
        for token in (
            "now - debounceStart < 28",
            "held >= 2800",
            "held >= 1200",
            "held >= 550",
            "held < 400",
            "now - lastTap < 380",
            "now - lastTap >= 380",
            "MENU_TIMEOUT_MS    16000UL",
        ):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE_V18)
                self.assertIn(token, SOURCE)

    def test_value_tables_match_v18(self):
        for token in (
            "{6, 12, 20, 30, 42, 56, 72, 88, 106, 125}",
            "{40, 55, 70, 85, 100, 120, 145, 170, 200, 240}",
            "SETTINGS_COUNT = 6",
        ):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE_V18)
                self.assertIn(token, SOURCE)

    def test_status_ball_matches(self):
        self.assertIn("28 + 8000 / (globalSpeed + 50)", SOURCE_V18)
        self.assertIn("28 + 8000 / (globalSpeed + 50)", SOURCE)

    def test_fireplace_spark_zone_is_scaled_not_copied(self):
        # V1.8 sparks at LEDs 5-8 of ten (upper fire, normalized 0.55-0.89).
        # A blind copy would put V1.9 sparks at 1-3 (lower-middle); the
        # scaled zone is LEDs 2-4 of five.
        self.assertIn("5 + entropy.range(4)", SOURCE_V18)
        spark = SOURCE[SOURCE.index("uint8_t spark =") :]
        spark = spark[: spark.index(";") + 1]
        self.assertIn("2 + entropy.range(2)", spark)

    def test_saber_gradient_is_scaled(self):
        # V1.8 falls off 11 per LED over ten; V1.9 falls off 22 per LED over
        # five: the same gradient, not the same constant.
        self.assertIn("(uint8_t)(i * 11)", SOURCE_V18)
        self.assertIn("(uint8_t)(i * 22)", SOURCE)

    def test_both_cursors_are_fixed_hot_pink(self):
        for src in (SOURCE, SOURCE_V18):
            cursor = src[src.index("void renderCursor(") :]
            cursor = cursor[: cursor.index("\n}\n")]
            with self.subTest(src="V1.9" if src is SOURCE else "V1.8"):
                self.assertIn("leds[index] = COLOR_HOT_PINK;", cursor)
                for banned in ("beatsin8(", "entropy.", "CHSV("):
                    self.assertNotIn(banned, cursor)



class FinalPassTests(unittest.TestCase):
    """Behaviour the final review found broken, pinned so it cannot return."""

    @staticmethod
    def _palette(src):
        start = src.index("uint8_t h = heat[i];")
        seg = src[start : start + 1500]
        thresholds = [int(v) for v in re.findall(r"h < (\d+)", seg)]
        offsets = [int(v) for v in re.findall(r"scale8\(\(uint8_t\)\(h - (\d+)\), 255\)", seg)]
        return thresholds, offsets

    def test_fireplace_palette_matches_v18(self):
        # A wrong offset in the h<55 branch (h - 55 instead of h - 20) was
        # shipped once; the palette must match V1.8 branch for branch.
        self.assertEqual(self._palette(SOURCE), self._palette(SOURCE_V18))
        thresholds, offsets = self._palette(SOURCE)
        self.assertEqual(thresholds, [20, 55, 100, 155, 210])
        self.assertEqual(offsets, [20, 55, 100, 155, 210])

    def test_toy_selection_leaves_the_home_menu(self):
        # Fireplace and Lightsaber set the mode but never left UI_MENU_MAIN
        # until the uiState assignment was added to their 5V branches.
        home = SOURCE[SOURCE.index("if (uiState == UI_MENU_MAIN) {") :]
        case1 = home[home.index("case 1:") : home.index("case 2:")]
        case2 = home[home.index("case 2:") : home.index("case 3:")]
        self.assertIn("modeFire.enter();", case1)
        self.assertIn("uiState = UI_MODE;", case1)
        self.assertIn("modeSaber.enter();", case2)
        self.assertIn("uiState = UI_MODE;", case2)

    def test_fireplace_keeps_the_top_led_for_sparks(self):
        self.assertIn("if (source > NUM_LEDS - 2) { source = NUM_LEDS - 2;", SOURCE)
        self.assertIn("1 + entropy.range(NUM_LEDS - 3)", SOURCE)
        self.assertRegex(SOURCE_V18, r"if \(source > 8\)\s+\{ source = 8;")
        self.assertIn("1 + entropy.range(6)", SOURCE_V18)

    def test_a_mode_flip_cannot_wake_the_board(self):
        flip = SOURCE[SOURCE.index("// Power-mode detection") :]
        flip = flip[: flip.index("// State transitions (armed only).")]
        # Both flip directions must be guarded, not just one of them.
        self.assertEqual(flip.count("if (uiState != UI_SLEEP) returnToIdle();"), 2)
        self.assertEqual(flip.count("returnToIdle();"), 2)

    def test_the_sleep_loop_is_not_throttled(self):
        # The ADS7128 state machine advances once per loop pass; a delay in
        # the sleep branch stretched each sweep to ~225 ms.
        loop = SOURCE[SOURCE.index("void loop() {") :]
        branch = loop[loop.index("if (uiState == UI_SLEEP) {") :]
        branch = branch[: branch.index("return;")]
        self.assertIn("updateShutdown();", branch)
        self.assertNotIn("delay(", branch)

    def test_mode_exit_uses_the_looser_rail_bound(self):
        self.assertRegex(SOURCE, r"#define\s+RAIL_MODE_STALE_US\s+500000UL")
        self.assertGreater(500000, 50000)
        self.assertIn("(uint32_t)(sensorNowUs - adsChTime[0]) <= RAIL_MODE_STALE_US", SOURCE)
        self.assertIn("inverterPower && (!railSeen || vIn < INVERTER_EXIT_V)", SOURCE)

    def test_state_changes_are_printed_once_by_powerservice(self):
        self.assertNotIn('Serial.println("STATE: WAIT_BUS")', SOURCE)
        self.assertNotIn('Serial.println("STATE: STARTING")', SOURCE)
        self.assertIn("Serial.println(powerStateName());", SOURCE)

    def test_arming_is_refused_outside_the_rail_window(self):
        # Arming with the rail outside 11-14 V used to latch an input-rail
        # fault 30 ms later, after nothing had been driven.  Refuse up front.
        arm = SOURCE[SOURCE.index("bool powerSetArmed(bool on)") :]
        arm = arm[: arm.index("void powerSerialReport()")]
        self.assertIn("if (!inputRailValid())", arm)
        self.assertIn("ARM refused: input rail out of 11-14 V", arm)
        self.assertLess(arm.index("if (!inputRailValid())"), arm.index("pArmed = true;"))

    def test_rand_header_is_explicit(self):
        self.assertIn('#include "pico/rand.h"', SOURCE)


if __name__ == "__main__":
    unittest.main()
