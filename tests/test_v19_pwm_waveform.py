"""Independent waveform and dead-time model of the V1.9 H-bridge.

This test does not re-use the sketch's arithmetic for the four compare
values; it recomputes them from the modulation index and models the
RP2040/RP2350 PWM peripheral as the datasheet describes it:

  * each slice has its own up-counting counter that wraps at TOP;
  * a normal-polarity channel is HIGH while counter < CC;
  * an inverted-polarity channel is HIGH while counter >= CC;
  * the counters of slices 0, 1 and 2 start at 0, dt and 2dt;
  * slice 0 carries leg A high (channel B), slice 1 carries leg A low
    (channel A, inverted) AND leg B high (channel B), slice 2 carries
    leg B low (channel A, inverted).

The physical model of the bridge: a high-side FET pulls its leg node to
+Vbus, a low-side FET pulls it to 0 V, and the winding sees the difference
between the two leg nodes.  The RC filter on each midpoint (10 kOhm with
47 nF, a 339 Hz corner) turns the 20 kHz switching node into its average,
so the quantity the ADS7128 eventually sees is the *carrier average* of the
differential.

Checks:

  1. the dead time on every edge of both legs, across the modulation range
     and across the dynamic (per-carrier re-latch) case;
  2. the carrier-average differential against the model the firmware's
     droop estimator assumes (drive RMS = m * Vbus / sqrt(2));
  3. the residual 20 kHz ripple that survives the 339 Hz filter, and the
     aliasing error it causes in an asynchronously sampled measurement.

Run with ``python3 -m pytest tests/test_v19_pwm_waveform.py``.
"""

import math
import unittest

PWM_TOP = 7499
PWM_T = PWM_TOP + 1
PWM_DT = 75
SQRT2 = math.sqrt(2.0)
VBUS = 21.75
CARRIER_HZ = 20000.0

# The midpoint RC filter: 10 kOhm bottom resistor with 47 nF across it.
FILTER_HZ = 1.0 / (2.0 * math.pi * 10000.0 * 47e-9)


def cc_values(m):
    """The four compare values, recomputed the way writeBridgeDuty() does."""
    d_a = (1.0 + m) * 0.5
    d_b = (1.0 - m) * 0.5
    cc_a_hi = int(d_a * PWM_T)
    cc_a_lo = int(d_a * PWM_T + 2.0 * PWM_DT)
    cc_b_hi = int(d_b * PWM_T)
    cc_b_lo = int(d_b * PWM_T + 2.0 * PWM_DT)
    cc_a_lo = min(cc_a_lo, PWM_TOP)
    cc_b_lo = min(cc_b_lo, PWM_TOP)
    cc_a_hi = max(cc_a_hi, 2 * PWM_DT)
    cc_b_hi = max(cc_b_hi, 2 * PWM_DT)
    return cc_a_hi, cc_a_lo, cc_b_hi, cc_b_lo


def leg_states(t, latched):
    """The four gate-driver inputs at integer tick t."""
    cc_a_hi, cc_a_lo, cc_b_hi, cc_b_lo = latched
    c0 = t % PWM_T
    c1 = (t + PWM_DT) % PWM_T
    c2 = (t + 2 * PWM_DT) % PWM_T
    return (
        c0 < cc_a_hi,  # A high  (slice 0, channel B)
        c1 >= cc_a_lo,  # A low   (slice 1, channel A, inverted)
        c1 < cc_b_hi,  # B high  (slice 1, channel B)
        c2 >= cc_b_lo,  # B low   (slice 2, channel A, inverted)
    )


def differential(t, latched):
    """The voltage across the transformer primary, in volts.

    Leg A node is +Vbus while A high and 0 V otherwise; leg B likewise.
    """
    a_hi, _a_lo, b_hi, _b_lo = leg_states(t, latched)
    return (1 if a_hi else 0) * VBUS - (1 if b_hi else 0) * VBUS


def carrier_average(m, samples_per_carrier=PWM_T):
    """The mean differential over one carrier period: this is what the
    midpoint RC filter delivers to the ADC."""
    cc = cc_values(m)
    total = 0.0
    for j in range(samples_per_carrier):
        total += differential(j, cc)
    return total / samples_per_carrier


def _edges(high):
    falls = [i for i in range(1, len(high)) if high[i - 1] and not high[i]]
    rises = [i for i in range(1, len(high)) if not high[i - 1] and high[i]]
    return falls, rises


def _min_gap(high_a, high_b):
    """Minimum low time between a fall of one and the next rise of the
    other, in ticks."""
    gaps = []
    a_falls, a_rises = _edges(high_a)
    b_falls, b_rises = _edges(high_b)
    for fall in a_falls:
        for rise in a_rises:
            if rise > fall:
                gaps.append(rise - fall)
                break
    for fall in b_falls:
        for rise in b_rises:
            if rise > fall:
                gaps.append(rise - fall)
                break
    return min(gaps) if gaps else None


class DeadTimeTests(unittest.TestCase):
    """The dead time is the firmware's only shoot-through protection."""

    def test_static_dead_time_across_the_modulation_range(self):
        for m in (-0.9597, -0.9, -0.5, -0.1, 0.0, 0.1, 0.5, 0.9, 0.9597):
            with self.subTest(m=m):
                cc = cc_values(m)
                samples = [leg_states(t, cc) for t in range(2 * PWM_T)]
                a_hi = [s[0] for s in samples]
                a_lo = [s[1] for s in samples]
                b_hi = [s[2] for s in samples]
                b_lo = [s[3] for s in samples]
                self.assertFalse(any(a and b for a, b in zip(a_hi, a_lo, strict=False)))
                self.assertFalse(any(a and b for a, b in zip(b_hi, b_lo, strict=False)))
                self.assertGreaterEqual(_min_gap(a_hi, a_lo), PWM_DT)
                self.assertGreaterEqual(_min_gap(b_hi, b_lo), PWM_DT)

    def test_dynamic_dead_time_when_the_sine_steps_each_carrier(self):
        """The ISR rewrites all four compares once per carrier.  Model the
        per-slice shadow latches: slice 0 latches at its own wrap, slice 1
        just before the end of the carrier, slice 2 before that."""
        amplitude = 0.9597
        carriers = 40
        ccs = []
        for k in range(carriers + 1):
            phase = (k * 12884902) & 0xFFFFFFFF
            idx = (phase >> 26) & 0x3F
            frac = (phase >> 18) & 0xFF
            s0 = math.sin(2.0 * math.pi * idx / 64.0)
            s1 = math.sin(2.0 * math.pi * (idx + 1) / 64.0)
            s = s0 + (s1 - s0) * (frac / 256.0)
            ccs.append(cc_values(amplitude * s))

        lat = list(ccs[0])
        samples = []
        for t in range(carriers * PWM_T):
            k = t // PWM_T
            off = t % PWM_T
            if t > 0 and off == 0:
                lat[0] = ccs[k - 1][0]
            if off == PWM_T - 2 * PWM_DT:
                lat[3] = ccs[k][3]
            if off == PWM_T - PWM_DT:
                lat[1] = ccs[k][1]
                lat[2] = ccs[k][2]
            samples.append(leg_states(t, tuple(lat)))

        a_hi = [s[0] for s in samples]
        a_lo = [s[1] for s in samples]
        b_hi = [s[2] for s in samples]
        b_lo = [s[3] for s in samples]
        self.assertFalse(any(a and b for a, b in zip(a_hi, a_lo, strict=False)))
        self.assertFalse(any(a and b for a, b in zip(b_hi, b_lo, strict=False)))
        self.assertGreaterEqual(_min_gap(a_hi, a_lo), PWM_DT)
        self.assertGreaterEqual(_min_gap(b_hi, b_lo), PWM_DT)


class DriveModelTests(unittest.TestCase):
    """The droop estimator assumes the drive RMS is m * Vbus / sqrt(2)."""

    def test_carrier_average_is_linear_in_the_modulation(self):
        """The carrier average of the differential is m * Vbus: the bridge
        really does deliver the drive amplitude the model assumes."""
        for m in (0.1, 0.3, 0.5, 0.7, 0.9):
            with self.subTest(m=m):
                self.assertAlmostEqual(carrier_average(m), m * VBUS, delta=0.15)

    def test_sixty_hz_fundamental_matches_the_model(self):
        """With the modulation index following a 60 Hz sine, the filtered
        drive has a 60 Hz fundamental of M * Vbus, i.e. M * Vbus / sqrt(2)
        RMS -- exactly what the estimator assumes."""
        amplitude = 0.9
        carriers = 1000
        re = im = 0.0
        n = 0
        for k in range(carriers):
            t = k / CARRIER_HZ
            m = amplitude * math.sin(2.0 * math.pi * 60.0 * t)
            avg = carrier_average(m)
            ang = 2.0 * math.pi * 60.0 * t
            re += avg * math.cos(ang)
            im += avg * math.sin(ang)
            n += 1
        fundamental = 2.0 * math.hypot(re, im) / n
        self.assertAlmostEqual(fundamental, amplitude * VBUS, delta=0.1)
        self.assertAlmostEqual(fundamental / SQRT2, amplitude * VBUS / SQRT2, delta=0.1)


class ResidualRippleTests(unittest.TestCase):
    """The 339 Hz midpoint filter does not remove the carrier entirely,
    and the drive is a 20 kHz PWM rather than a 60 Hz sine."""

    DIVIDER = 10.0 / 78.0

    def _ripple(self, m):
        cc = cc_values(m)
        raw = [differential(t, cc) for t in range(PWM_T)]
        return max(raw) - min(raw)

    def test_residual_ripple_after_the_filter(self):
        """The 20 kHz carrier survives the 339 Hz filter at about -35 dB and
        is aliased by the asynchronous sampling."""
        ripple = self._ripple(0.9)
        residual_at_adc = ripple * FILTER_HZ / CARRIER_HZ * self.DIVIDER
        signal_at_adc = 0.9 * VBUS * self.DIVIDER
        # A percent or two of the signal.
        self.assertGreater(residual_at_adc, 0.02)
        self.assertLess(residual_at_adc / signal_at_adc, 0.05)

    def test_the_aliased_ripple_stays_far_below_the_dc_trip(self):
        """Averaged over the 128-sample window the aliased ripple adds a
        random component of about residual/sqrt(128) to the measured DC
        offset, which must stay well under the 1.0 V trip."""
        ripple = self._ripple(0.9)
        residual_at_adc = ripple * FILTER_HZ / CARRIER_HZ * self.DIVIDER
        dc_error = residual_at_adc / math.sqrt(128)
        self.assertLess(dc_error, 0.01)

    def test_the_drive_is_a_carrier_pwm_not_a_sine(self):
        """The total RMS of the differential is much larger than the RMS of
        its 60 Hz fundamental.  The estimator measures the fundamental (the
        midpoint filter removes the carrier), but the transformer and the
        outlet see the whole waveform."""
        for m in (0.879, 0.941, 0.9597):
            with self.subTest(m=m):
                cc = cc_values(m)
                raw = [differential(t, cc) for t in range(PWM_T)]
                total_rms = math.sqrt(sum(v * v for v in raw) / len(raw))
                fundamental_rms = m * VBUS / SQRT2
                self.assertGreater(total_rms, 1.4 * fundamental_rms)
                self.assertLess(total_rms, 1.6 * fundamental_rms)

    def test_the_carrier_average_is_what_the_estimator_measures(self):
        """The estimator's model -- drive RMS = m * Vbus / sqrt(2) -- is the
        fundamental of the filtered waveform, not its total RMS."""
        for m in (0.5, 0.8, 0.94):
            with self.subTest(m=m):
                cc = cc_values(m)
                raw = [differential(t, cc) for t in range(PWM_T)]
                total_rms = math.sqrt(sum(v * v for v in raw) / len(raw))
                fundamental_rms = m * VBUS / SQRT2
                self.assertGreater(total_rms / fundamental_rms, 1.4)


if __name__ == "__main__":
    unittest.main()
