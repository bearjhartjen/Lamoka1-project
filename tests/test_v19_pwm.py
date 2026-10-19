"""Host-side model of the V1.9 three-slice PWM and carrier ISR.

The H-bridge has no hardware dead-time generator.  V1.9 builds the dead time
from three phase-offset PWM slices and a delayed compare on each low side, and
it writes the sine once per 20 kHz carrier from the slice-0 wrap ISR.  This
test models that scheme tick by tick and checks the two properties that
matter:

* the high and low sides of each leg are never on together;
* the gap on each edge stays at least ``PWM_DT_TICKS`` (500 ns at 150 MHz).

The dynamic model includes the per-slice shadow latches.  That is the
regression for the old main-loop update: when the main loop was slow, the two
phase-offset slices could latch different duty values and open a notch in the
dead time.  The fixed test target is the 60 Hz zero crossing at full
modulation, where the per-carrier sine step is largest.

Run with ``python3 -m unittest tests.test_v19_pwm`` or let pytest discover it.
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


PWM_TOP = _int("PWM_TOP")
PWM_DT_TICKS = _int("PWM_DT_TICKS")
SINE_TABLE_SIZE = _int("SINE_TABLE_SIZE")
PWM_T = PWM_TOP + 1
PWM_M_MAX = (PWM_T - 4 * PWM_DT_TICKS - 2) / PWM_T

assert re.search(r"#define\s+SINE_PHASE_INC\s+12884902UL", SOURCE)
SINE_PHASE_INC = 12884902
assert SINE_TABLE_SIZE == 64
assert PWM_TOP == 7499 and PWM_DT_TICKS == 75


def clamp(value, low, high):
    return low if value < low else high if value > high else value


def cc_values(m):
    """The four compare values written by writeBridgeDuty()."""
    m = clamp(m, -PWM_M_MAX, PWM_M_MAX)
    duty_a = (1.0 + m) * 0.5
    duty_b = (1.0 - m) * 0.5

    cc_a_hi = int(duty_a * PWM_T)
    cc_a_lo = int(duty_a * PWM_T + 2.0 * PWM_DT_TICKS)
    cc_b_hi = int(duty_b * PWM_T)
    cc_b_lo = int(duty_b * PWM_T + 2.0 * PWM_DT_TICKS)

    cc_a_lo = min(cc_a_lo, PWM_TOP)
    cc_b_lo = min(cc_b_lo, PWM_TOP)
    cc_a_hi = max(cc_a_hi, 2 * PWM_DT_TICKS)
    cc_b_hi = max(cc_b_hi, 2 * PWM_DT_TICKS)
    return cc_a_hi, cc_a_lo, cc_b_hi, cc_b_lo


def outputs_at(t, latched_a_hi, latched_a_lo, latched_b_hi, latched_b_lo):
    """Sample the four gate-driver inputs at integer tick t.

    Slice 0 (A high) starts at 0; slice 1 (A low, B high) starts at +dt;
    slice 2 (B low) starts at +2dt.  The low-side channels are inverted.
    """
    c0 = t % PWM_T
    c1 = (t + PWM_DT_TICKS) % PWM_T
    c2 = (t + 2 * PWM_DT_TICKS) % PWM_T
    a_hi = c0 < latched_a_hi
    a_lo = c1 >= latched_a_lo
    b_hi = c1 < latched_b_hi
    b_lo = c2 >= latched_b_lo
    return a_hi, a_lo, b_hi, b_lo


def _edges(high):
    """Falling and rising indices of a boolean sequence (wrap not included)."""
    falls = [i for i in range(1, len(high)) if high[i - 1] and not high[i]]
    rises = [i for i in range(1, len(high)) if not high[i - 1] and high[i]]
    return falls, rises


def _min_gap(high_a, high_b, start, stop):
    """Minimum edge-to-edge low time between a fall of one and a rise of the other."""
    gaps = []
    a_falls, a_rises = _edges(high_a)
    b_falls, b_rises = _edges(high_b)
    for fall in a_falls:
        if not (start <= fall < stop):
            continue
        for rise in b_rises:
            if rise > fall:
                # The falling edge is at tick `fall`; the rising edge is at
                # tick `rise`.  The dead time is the difference, not the
                # number of fully-low samples.
                gaps.append(rise - fall)
                break
    for fall in b_falls:
        if not (start <= fall < stop):
            continue
        for rise in a_rises:
            if rise > fall:
                gaps.append(rise - fall)
                break
    return min(gaps) if gaps else None


def sine_from_table(phase):
    """The interpolation used by onPwmWrap(), with a 64-point sine table."""
    index = (phase >> 26) & 0x3F
    frac = (phase >> 18) & 0xFF
    s0 = math.sin(2.0 * math.pi * index / 64.0)
    s1 = math.sin(2.0 * math.pi * (index + 1) / 64.0)
    return s0 + (s1 - s0) * (frac / 256.0)


class StaticPwmTests(unittest.TestCase):
    def test_no_overlap_and_500ns_gap_across_the_range(self):
        for m in (-PWM_M_MAX, -0.9, -0.5, -0.1, 0.0, 0.1, 0.5, 0.9, PWM_M_MAX):
            with self.subTest(m=m):
                cc = cc_values(m)
                samples = [outputs_at(t, *cc) for t in range(2 * PWM_T)]
                a_hi = [s[0] for s in samples]
                a_lo = [s[1] for s in samples]
                b_hi = [s[2] for s in samples]
                b_lo = [s[3] for s in samples]

                self.assertFalse(any(a and b for a, b in zip(a_hi, a_lo, strict=False)))
                self.assertFalse(any(a and b for a, b in zip(b_hi, b_lo, strict=False)))

                gap_a = _min_gap(a_hi, a_lo, PWM_T, 2 * PWM_T)
                gap_b = _min_gap(b_hi, b_lo, PWM_T, 2 * PWM_T)
                self.assertIsNotNone(gap_a)
                self.assertIsNotNone(gap_b)
                self.assertGreaterEqual(gap_a, PWM_DT_TICKS)
                self.assertGreaterEqual(gap_b, PWM_DT_TICKS)

    def test_m_max_uses_the_whole_allowed_band(self):
        cc = cc_values(PWM_M_MAX)
        # dA + dB = 1, so the two high-side compares add to one period; the
        # low-side compares each carry the +2*dt offset.
        self.assertEqual(cc[0] + cc[2], PWM_T)
        self.assertEqual(cc[1] + cc[3], PWM_T + 4 * PWM_DT_TICKS)


class DynamicPwmTests(unittest.TestCase):
    """Model the carrier ISR and the per-slice shadow latches."""

    def _run(self, carriers):
        # The ISR at t=k*T increments the phase, then writes cc(k).  The model
        # starts at full amplitude so the zero-crossing step is the worst case.
        ccs = []
        for k in range(carriers + 1):
            phase = ((k + 1) * SINE_PHASE_INC) & 0xFFFFFFFF
            m = PWM_M_MAX * sine_from_table(phase)
            ccs.append(cc_values(m))

        latched_a_hi = ccs[0][0]
        latched_a_lo = ccs[0][1]
        latched_b_hi = ccs[0][2]
        latched_b_lo = ccs[0][3]

        samples = []
        for t in range(carriers * PWM_T):
            k = t // PWM_T
            offset = t % PWM_T
            # Slice 0 latches the value written one carrier earlier at its
            # own wrap (the start of this carrier).
            if t > 0 and offset == 0:
                latched_a_hi = ccs[k - 1][0]
            # Slice 2 and slice 1 latch the current carrier's value near the
            # end of the carrier, in that order.
            if offset == PWM_T - 2 * PWM_DT_TICKS:
                latched_b_lo = ccs[k][3]
            if offset == PWM_T - PWM_DT_TICKS:
                latched_a_lo, latched_b_hi = ccs[k][1], ccs[k][2]

            samples.append(outputs_at(t, latched_a_hi, latched_a_lo, latched_b_hi, latched_b_lo))
        return samples

    def test_dynamic_dead_time_at_the_zero_crossing(self):
        samples = self._run(5)
        a_hi = [s[0] for s in samples]
        a_lo = [s[1] for s in samples]
        b_hi = [s[2] for s in samples]
        b_lo = [s[3] for s in samples]

        self.assertFalse(any(a and b for a, b in zip(a_hi, a_lo, strict=False)))
        self.assertFalse(any(a and b for a, b in zip(b_hi, b_lo, strict=False)))

        gap_a = _min_gap(a_hi, a_lo, PWM_T, 4 * PWM_T)
        gap_b = _min_gap(b_hi, b_lo, PWM_T, 4 * PWM_T)
        self.assertIsNotNone(gap_a)
        self.assertIsNotNone(gap_b)
        self.assertGreaterEqual(gap_a, PWM_DT_TICKS)
        self.assertGreaterEqual(gap_b, PWM_DT_TICKS)


class SineStepTests(unittest.TestCase):
    def test_per_carrier_step_is_inside_the_dead_time_band(self):
        # 60 Hz sampled at 20 kHz: the largest continuous step is 2*pi*60/20k
        # times the amplitude.  At M_MAX that is about 0.0181, below the
        # 2*dt/T = 0.020 band where the phase-offset compares can breach.
        ideal = PWM_M_MAX * 2.0 * math.pi * 60.0 / 20000.0
        self.assertAlmostEqual(ideal, 0.0181, places=3)
        self.assertLess(ideal, 2.0 * PWM_DT_TICKS / PWM_T)

    def test_table_interpolation_step_over_one_cycle(self):
        # Walk one 60 Hz cycle of the actual 32-bit phase accumulator and
        # table interpolation; the largest neighbouring step must stay below
        # the band.
        limit = 2.0 * PWM_DT_TICKS / PWM_T
        previous = None
        worst = 0.0
        for k in range(340):
            phase = (k * SINE_PHASE_INC) & 0xFFFFFFFF
            m = PWM_M_MAX * sine_from_table(phase)
            if previous is not None:
                worst = max(worst, abs(m - previous))
            previous = m
        self.assertLess(worst, limit)
        self.assertGreater(worst, 0.015)

    def test_slow_main_loop_would_breach_the_band(self):
        # The old code wrote once per main loop.  At 5 kHz (200 us) the step
        # is 4x the allowed band; this is why the update moved into the ISR.
        slow_step = PWM_M_MAX * 2.0 * math.pi * 60.0 * 200e-6
        self.assertGreater(slow_step, 2.0 * PWM_DT_TICKS / PWM_T)
        self.assertAlmostEqual(slow_step, 0.0724, places=3)

    def test_phase_increment_matches_60_hz_at_20_khz(self):
        expected = round(60.0 / 20000.0 * 2**32)
        self.assertEqual(expected, SINE_PHASE_INC)


if __name__ == "__main__":
    unittest.main()
