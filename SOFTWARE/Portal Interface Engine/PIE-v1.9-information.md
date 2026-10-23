## Lamoka1/2 — PIE Version V1.9

The **Lamoka1/2 PIE Version V1.9** is the first Portal Interface Engine release that runs
on the main Lamoka1/2 board. It has two personalities, switched automatically by the
measured input rail: on USB 5 V alone (rail ~0 V) it is the V1.8 toy set on five LEDs
(Status, Fireplace, Lightsaber, Rainbow, Settings); with a battery on the rail
(>=10 V) the toy modes are replaced by the Inverter portal and the home menu becomes
Status, Inverter, Settings. The H-bridge power stage only exists in inverter mode:
the LM5122 boost is left to run in hardware, while the RP2354A generates the four
gate-driver PWM signals, reads the ADS7128 monitor chain, and runs the inverter's
control and protection loops. V1.9 is **not** DEV-Brain firmware and does not run
unmodified on the Lamoka1-DEV-Brain.

The firmware is written in Arduino C++ using the **FastLED** library for LED output,
the Raspberry Pi Pico SDK (`hardware/pwm`, `hardware/adc`, `hardware/watchdog`) for the
power stage, `Wire` for the ADS7128, and `pico/bootrom.h` for `reset_usb_boot()`.

### Hardware Interface

V1.9 is written for the main Lamoka1/2 board (per `HARDWARE/COMPONENTS.md`, MCU is the
RP2354A made by Raspberry Pi):

* **GPIO 11** — WS2812B LED data (D1–D5, five LEDs)
* **GPIO 0** — User button (B3F-4055), `INPUT_PULLUP`
* **GPIO 8 / 9** — I²C0 SDA/SCL to the ADS7128 monitor (U10), 400 kHz
* **GPIO 26** — `TEMP_CTRL`, TMP235 output (control-section temperature)
* **GPIO 27** — `TEMP_INV`, TMP235 output (inversion-section temperature)
* **GPIO 1–4** — Gate-driver inputs to the UCC27735 (Q3–Q6 H-bridge)
* **GPIO 5, 28, 29** — unconnected; must not be driven

The firmware controls **5 WS2812B LEDs** as a single FastLED array in **GRB** colour
order. FastLED dithering is deliberately disabled.

> [!NOTE]
> This is main-board firmware. `NUM_LEDS` is `5`, matching the Lamoka1/2 (D1–D5).
> The DEV-Brain fits ten LEDs and wires GPIO 5/26–29 differently, so this sketch does
> not run on the DEV-Brain unmodified. See `SOFTWARE/README.md` for the pin-by-pin
> comparison.

The default operating values are:

* **Brightness:** 6
* **Speed:** 100
* **Frame interval:** 16 ms
* **Menu timeout:** 16 seconds
* **Minimum brightness:** 6

### One-Button Interface

The entire user interface is controlled through the single button on GPIO 0, using
`INPUT_PULLUP`. V1.9 measures press length and timing, recognising five gestures:

* **Tap** — a short press
* **Double tap** — two taps within 380 ms
* **Hold** — approximately 550 ms
* **Long hold** — approximately 1200 ms
* **Very long hold** — approximately 2800 ms

Button input has a **28 ms debounce period**. A tap behaves slightly differently
depending on whether the user is already inside a menu: outside a menu the firmware
waits to see whether it becomes a double tap; inside a menu it is returned immediately
as a selection gesture.

### User Interface State Machine

V1.9 is organised around eight UI states:

* **Mode** — status display (the only lighting mode fitted)
* **Main Menu**
* **Settings Menu**
* **Brightness**
* **Speed**
* **Info**
* **Inverter** (new in V1.9 — the power-stage portal)
* **Sleep**

The settings menu remembers its cursor position, so returning to Settings does
not reset the cursor.

### Main Menu

The main menu depends on the power mode.

Inverter mode (battery on the rail) has three positions, each with its own
colour in D1–D3 order:

1. **Status** — blue
2. **Inverter** — purple
3. **Settings** — yellow-orange

5V mode (USB power alone) has five positions in V1.8 colours:

1. **Status**
2. **Fireplace**
3. **Lightsaber**
4. **Rainbow**
5. **Settings**

Positions 0–3 use the cactus-green menu colour and Settings is blue.
The selected position is displayed in steady **hot pink** — no pulse, no hue
variation. (The DEV-Brain cursor was seen drifting toward near-white, which
reads as a system bug; on this board selection must never look like a fault.) A tap moves forward, a double tap moves backward, a hold selects, and a
long hold returns to the default idle state. Neither menu needs scrolling on five LEDs
window scrolls so the cursor is always visible.

### Default Status Mode

The default idle display is a **blue bouncing ball** moving across the five LEDs,
with two trailing LEDs for a short blue trail. The movement rate is affected by the
global speed setting. Status mode is selected automatically at startup and on return
from shutdown, menu timeout, or the appropriate long-hold action.

### Inverter Portal (new in V1.9)

The **Inverter** home item is the power-stage portal. It shows the inverter state on
the five LEDs and is the only place the bridge can be armed or disarmed:

* **Faulted** — pulsing red (all five LEDs).
* **No sensors** (ADS7128 not responding) — amber dot on LED 0.
* **Disarmed** — dim blue standby dot on LED 0.
* **Waiting for bus** (armed, 21.75 V bus not yet valid) — amber pulse on LED 0.
* **Starting** (soft-start ramp) — green bar growing left to right, tracking the
  bus voltage.
* **Running** — green bar whose height tracks the output regulation (0–120 V RMS).
* **Armed, input rail invalid** — dim amber on LED 0.

Gestures in the Inverter portal:

* **Hold** — if a fault is latched, clear it (over-faults are refused); otherwise
  arm (sensors required, inverter mode only) or disarm.
* **Double tap** — clear a non-over fault.
* **Long hold** — disarm, then return to the home menu (cursor stays on Inverter).
* **Tap** — no action (status display only). The portal is treated as a non-menu
  for gesture synthesis, so a tap here is delayed by the 380 ms double-tap window;
  this is what produces the double-tap fault clear.

The bridge is **never energised without an explicit arm action**. A valid voltage
reading is not treated as permission to switch. Idle means disarmed: leaving the
portal, the menu timeout, sleep mode and BOOTSEL entry all disarm the bridge
before acting. The only exception is a latched fault, where the state stays
`FAULT` until it is cleared or the MCU is reset.

### Power modes (5V vs inverter)

The board has two personalities, switched automatically by the measured input
rail. USB 5 V alone runs the MCU, the LEDs and the ADS7128, but the rail reads
~0 V: that is **5V mode** (Status, Fireplace, Lightsaber, Rainbow, Settings —
the V1.8 toy set in V1.8 colours). A battery on the rail flips the board into
**inverter mode** (Status blue, Inverter purple, Settings amber), where the toy
modes are replaced by the Inverter portal.

The flip has hysteresis. Entering inverter mode needs 10.0 V or more on a fresh
AIN0 sample. Leaving it needs AIN0 below 9.0 V, or no AIN0 reading for 0.5 s
(`RAIL_MODE_STALE_US`). The 0.5 s bound is looser than the 50 ms sensor limit on
purpose: a loop stall such as the 120 ms very-long-hold flash cannot flip the
mode. At boot, before the first sweep, the board is in 5V mode, which is the
safe side. Either flip disarms the bridge and returns to idle, because the home
menu has a different item count per mode and no cursor survives it. A flip
while asleep changes the mode but does not wake the board.

A battery removed or drained below 9.0 V is not a fault: the bridge disarms and
nothing latches, so re-inserting the battery needs no fault clear. A sag that
stays between 9.0 V and 10.8 V while armed latches an input-rail fault after
30 ms; clear it from the Inverter portal.

Arming is refused in 5V mode (`ARM refused: no battery (5V mode)`); the
Inverter portal is unreachable there. The one-second serial report carries
`MODE=5V` or `MODE=INV`.

### Lighting modes (5V mode only)

Fireplace, Lightsaber and Rainbow behave as in V1.8, scaled to five LEDs:
a 5-element heat model with flares, a hue-guaranteed-different blade with
patterns and clash, and `fill_rainbow()` with ten tap-cycled speeds. They are
only offered while no battery is on the input rail; a battery replaces them with
the Inverter portal. As in V1.8, the fire's source stays off the top LED, which
is reserved for sparks: the source and the fire's targets both stay on LEDs 1-3.

### Games

There are none anywhere in V1.9: no Fireball game, no Games menu. (V1.8 on the
DEV-Brain keeps them.)

### Settings Menu

Six positions: Back, Brightness, Speed, Info, BOOTSEL, Shutdown — same as V1.8.

### Brightness Control

Ten predefined levels: `6, 12, 20, 30, 42, 56, 72, 88, 106, 125`. The current level
is shown as a 5-LED bar (two levels per LED: hot pink up to the selected level,
cactus green above). Tap moves up, double tap moves down. Hard minimum brightness of
**6**.

### Speed Control

Ten predefined values: `40, 55, 70, 85, 100, 120, 145, 170, 200, 240`, shown on the
same 5-LED bar. Tap moves up, double tap moves down.

### Information Screen

Three pages (all-LED cactus with LED 0 hot pink; an animated count of lit LEDs based
on the seconds counter; all-LED cactus). Tap advances, double tap goes back, hold
returns to the Settings menu. It does not clear a fault — that is done from the
Inverter portal.

### Entropy and Randomness

V1.9 uses the RP2354A's hardware TRNG (`get_rand_64()`); the TMP235 analog
channels are temperature sensors, not entropy sources. An `EntropyService`
mixes the hardware value with `micros()` (300 mixing operations at init,
updated every loop) and provides 32-bit, 8-bit, ranged and boolean values.

Entropy stays in the 5V lighting modes only — fireplace flames, lightsaber
hue and flicker, rainbow start hue. It never touches selection (the cursor is
a fixed hot pink) and never touches the power stage: no protection threshold,
trip, measurement or modulation value reads the entropy service.

### Menu Timeout

**16 seconds** of inactivity returns the interface to the default blue Status mode,
resetting the main mode and cursor.

### Very-Long-Hold Function

A 2.8 s hold disarms the bridge first, then fills all five LEDs with hot pink for
120 ms and forces the interface back to the default Status mode.

### Shutdown Mode

Software sleep: LEDs cleared, brightness zero, wait for button release, then wake on
a 1.5 s hold, restoring brightness and Status mode. The bridge is disarmed on entry. The loop is not throttled while asleep, so the
input sensors keep sweeping and a battery arriving during sleep is noticed; the
power mode changes, but only the 1.5 s hold wakes the board.

### USB BOOTSEL Mode

Clears the LEDs, waits 40 ms, disables the hardware watchdog (otherwise it
would reset the chip out of BOOTSEL ~500 ms later), and calls
`reset_usb_boot(0, 0)`. The bridge is disarmed first.

### Startup Sequence

1. `powerBegin()` parks all four gate-driver inputs low (SIO outputs, LOW) before
   anything else.
2. FastLED is initialised for the five WS2812B LEDs; dither disabled; brightness
   applied; LEDs cleared.
3. The entropy and button services are initialised (entropy feeds the 5V toy modes only).
4. A short startup sweep across all five LEDs (HSV hues, 18 ms per LED), feeding
   the watchdog after each step, then a 70 ms delay with a final watchdog feed.
5. The interface enters the default Status mode.

The ADS7128 is probed at 400 kHz first. A chip that answers only at 100 kHz is
detected and reported (`WARN: ADS7128 only at 100 kHz; arming disabled`), but the
bridge cannot be armed on that bus; the 400 kHz probe is retried every 500 ms
while the bridge is disarmed. The I²C timeout is capped at 2 ms so a stuck bus
cannot block the loop. A missing chip is reported on Serial and disables arming
but does not latch a fault.

### Main Program Loop

On every iteration:

1. `powerService(now)` — watchdog feed, ADS probe retry and sampling, temperature
   reads, RMS update, 100 Hz control loop, state machine, serial reporting. The PWM
   compares are not written here: the 20 kHz carrier-wrap ISR advances the sine
   and writes all four compares once per carrier, while the control tick only sets
   the amplitude.
2. If in sleep mode, run the shutdown update and return.
3. Update the entropy service.
4. Check the menu timeout.
5. Poll the button and dispatch gestures.
6. Render the current UI state and call `FastLED.show()` at the 16 ms frame interval.

### Power Stage

#### Topology

Battery 11–14 V → `Rsense1` → LM5122 boost → **21.75 V bus** → UCC27735 gate
drivers → Q3/Q4/Q5/Q6 H-bridge → 12 V winding of a Triad **VPT24-6670** → 115 V
winding → **120.0 V RMS regulated outlet**. The LM5122 runs in hardware; the MCU
only generates PWM. The U5/U6 VDD rails and the bootstrap diodes run from CSP
(the 11–14 V battery rail), not the 21.75 V bus.

The bus is set by the LM5122's own 1.2 V feedback reference:
`1.2 × (1 + 64.9/3.79) = 21.749 V`, with `Rfbt1` = 64.9 kΩ and `Rfbb1` =
3.79 kΩ. With 1 % resistors and a 1.188/1.2/1.212 V reference the tolerance
band is 21.13 V to 22.39 V.

#### How the outlet is regulated

**The board has no isolated output sense**, so the firmware regulates an
*estimate* of the outlet rather than a measurement of it. It:

1. measures the **drive winding** (12 V) RMS from the bridge midpoint
   differential (`AIN2 − AIN3`);
2. estimates the output power from the battery-side power (`AIN0` × `AIN4`),
   less the auxiliary load, times an efficiency constant;
3. estimates the outlet from the drive and the load with the transformer's
   series impedance (droop compensation):

   ```
   V_out_est = A_DROOP × V_drive_rms − B_DROOP × I_sec_est
   ```

   solved in closed form (see `outletEstimate()`);
4. closes the loop on `V_out_est`.

`A_DROOP` is the transformer's **no-load** ratio and `B_DROOP` its series
impedance referred to the 115 V secondary. The nameplate 12.0 V is a
**full-load** rating, so the open-circuit secondary is 12.0 × 1.08 = 12.96 V and

```
A_DROOP = 115 / 12.96 = 8.8735
B_DROOP = (12.96 − 12.0)/13.34 × A_DROOP² = 5.666 Ω
```

`115/12 = 9.583` is the *full-load* ratio and is wrong here: it overstates the
outlet by 8 %, so the firmware would believe the outlet was at 120 V when it
was really at about 111.5 V.

**Both constants are the transformer's catalogue values, used as they come.**
Nobody fits them to their own unit and nobody has to calibrate anything.

How far off can that be? The one thing the model does not know is the unit's own
regulation *r*: the catalogue says 8 % TYP with no published maximum. For a
transformer regulating *r*, the no-load secondary is `12.0 × (1 + r)` V, the
no-load ratio is `k = 115 / (12.0 × (1 + r))` and the series impedance referred to
the 115 V winding is `Z = (12.0 × r / 13.34) × k²`. The firmware asks for the
drive that satisfies the model at 120.0 V, and the real outlet is then
`k × V_drive − Z × I_sec`:

| regulation *r* | *k* | *Z* (Ω) | 0 W | 25 W | 50 W | 75 W | 100 W |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 6 % | 9.0409 | 4.412 | 122.26 | 122.55 | 122.83 | 123.11 | 123.40 |
| 7 % | 8.9564 | 5.051 | 121.12 | 121.26 | 121.40 | 121.54 | 121.68 |
| 8 % | 8.8735 | 5.666 | 120.00 | 120.00 | 120.00 | 120.00 | 120.00 |
| 9 % | 8.7920 | 6.258 | 118.90 | 118.76 | 118.63 | 118.50 | 118.36 |
| 10 % | 8.7121 | 6.828 | 117.82 | 117.55 | 117.29 | 117.03 | 116.76 |
| 11 % | 8.6336 | 7.376 | 116.76 | 116.37 | 115.98 | 115.59 | 115.20 |

So the catalogue constants hold the outlet **within 3 % of 120.0 V RMS** across
the whole load range for any transformer regulating between **6 % and 10 %**,
which brackets the 8 % TYP value and covers the spread between units out of the
same box. At 11 % the error reaches −4 %.

That is the **droop-model term only**, and it assumes the drive RMS is measured
accurately. It is not the as-built accuracy: the ADS7128 sweep adds a few
percent (see Measurements), the 3.3 V rail is ±1–2 %, and the dividers are 1 %,
so the total without a per-unit calibration is about **±8 %**, not ±3 %. A
meter on the outlet is the only way to know the real voltage, and a per-unit
calibration is the only way to tighten it.

The estimate of the output *power* is about 12 % high (`ETA_BUCK_HBRIDGE`
assumes 0.92 for the whole chain where the transformer alone is 0.89); that
error is proportional to the load, so it lands in the same place as the droop
error and needs no separate correction.

The droop model is the largest single term, but the measurement chain is not
calibrated. Until a unit is checked against a meter, treat the outlet as
accurate to about **±8 %**, not ±3 %. If it reads outside that, check the
transformer wiring and the boost bus before blaming the constants.

> [!CAUTION]
> **Nothing on this board senses the 120 V RMS side.** There is no voltage and
> no current measurement after the transformer, so nothing in the firmware can
> see what the 115 V winding is doing. The outlet voltage is an estimate, and
> the only way to know the real outlet voltage is to measure it with a meter.
> Treat the output as live mains and do not expect the firmware to notice a
> fault on the 115 V side.

A direct isolated output sense (an AMC1311B) comes with a later Lamoka1 model.
Until it does, the 125 V receptacle ceiling is enforced on the estimate.

#### PWM dead-time scheme

The H-bridge has no hardware dead-time generator; the UCC27735 has interlock but no
built-in dead time (datasheet §6.3.1), so dead time is entirely firmware's job.
The scheme (checked by the host-side tests `tests/test_v19_pwm.py` — PWM/dead-time
model — `tests/test_v19_pwm_waveform.py` — an independent model of the PWM
peripheral, its dead time and the resulting drive waveform — and
`tests/test_v19_model.py` — numeric/source checks):

| GPIO | Leg / side | PWM slice / channel | Polarity | Compare (CC) |
| --- | --- | --- | --- | --- |
| 1 | A high | slice 0, channel B | non-inverted | `dA·T` |
| 2 | A low  | slice 1, channel A | inverted    | `dA·T + 2·dt` |
| 3 | B high | slice 1, channel B | non-inverted | `dB·T` |
| 4 | B low  | slice 2, channel A | inverted    | `dB·T + 2·dt` |

All three slices share `TOP = 7499` (T = 7500 ticks), divider = 1 (150 MHz PWM
clock), up-counting mode. Before enabling, the counters are phased:

* slice 0 CTR = 0
* slice 1 CTR = +dt (75 ticks)
* slice 2 CTR = +2·dt (150 ticks)

with `dt = 75` ticks = 500 ns. The inverted-channel compares are delayed by
`2·dt` and rounded to whole ticks (truncation would shave up to a tick off the
programmed gap). Combined with the phase offsets this gives each leg a **500 ns
gap on both edges** for any duty in the allowed band, which is why `|m|` is clamped to
`PWM_M_MAX ≈ 0.9597`. The host-side model `tests/test_v19_pwm.py` confirms zero
overlap and ≥ 500 ns gaps for both legs across the full modulation range,
including the extremes; `tests/test_v19_pwm_waveform.py` re-derives the same
result from the peripheral's own semantics rather than from the sketch's
constants, and covers the dynamic case where the sine is rewritten every
carrier and the slices latch at different instants; `tests/test_v19_model.py`
checks the numeric constants and their sources.

Note: GPIO 2 and GPIO 3 share slice 1 and therefore share one counter/phase. This
is not a defect — the scheme does not require a per-channel phase inside a slice;
the second edge's delay comes from the per-channel compare offset (`+2·dt`), which
is independent per channel even on a shared slice.

#### What the transformer winding actually sees

This is the one part of the modulation that is easy to misread, so it is worth
stating plainly: **the drive is a 20 kHz unipolar PWM with the 60 Hz
fundamental riding on it — not a 60 Hz sine, and not a bipolar 20 kHz square
wave.**

The two legs are nearly in phase, not 180° apart: slice 1 carries leg A's low
side *and* leg B's high side, and the three counters are only `dt` and `2·dt`
apart. The differential across the primary is therefore a one-sided pulse train
whose per-carrier average is `m · Vbus`.

Two consequences, and they pull in opposite directions:

* **The regulation model is correct.** The 339 Hz filter on each midpoint
  (C29/C30 across the 10 kΩ bottom resistor) turns the switching node into its
  average, so what the ADS7128 measures is the 60 Hz fundamental. Its RMS is
  `m · Vbus / √2` — exactly what the droop estimator assumes. The host-side
  model `tests/test_v19_pwm_waveform.py` confirms this tick by tick: the
  carrier-average differential is exactly `m · Vbus`, and the 60 Hz fundamental
  of the drive is `M · Vbus`.
* **The transformer and the outlet see the whole waveform.** Over a mains
  cycle the modulation is a sine, so the cycle mean square is
  `Vbus² · mean|m| = Vbus² · 2A/π` and the total RMS of the differential is
  about **1.16× its 60 Hz fundamental** at the design point — 16.8 V against
  14.5 V at m = 0.941. (A constant-m carrier gives a larger ratio, √(2/m),
  which is not the operating case.) The winding currents are unaffected
  (they are set by the load and stay sinusoidal: 6.9 A of 13.34 A on the 12 V
  side at 100 W), and the carrier's volt-seconds balance over every carrier
  period, so the core does not walk up — but there is 20 kHz core loss and the
  outlet carries the carrier. A later board that puts each leg on its own
  slice (leg A on slice 1 = GPIO2/GPIO3, leg B on slice 2 = GPIO4/GPIO5, the
  two slices 180° apart) would give a true bipolar drive; that is a
  pin-assignment change, not a firmware change.

#### Carrier-synchronous sine generation

The modulation is not computed in the main loop. A 60 Hz phase advances in the
**20 kHz carrier-wrap ISR on slice 0** (`onPwmWrap`, the exclusive `PWM_IRQ_WRAP`
handler), which reads a 64-point sine table with linear interpolation and writes
all four compare registers once per carrier period. The main loop's 100 Hz
control tick only updates the amplitude (`modAmp`). Slice 0 wraps last in the
three-slice sequence, so the ISR runs after all three have wrapped and leaves
nearly a full carrier before the next slice latches; all three then latch the
same value at their next wraps. Main-loop jitter therefore cannot make the
phase-offset slices latch different duty values, and the 500 ns gap holds even
when the loop is busy. Interpolation keeps the per-carrier amplitude step close
to the 60 Hz derivative (~0.019 at m = 1) rather than the coarser raw table
step, keeping the duty inside the dead-time band.

#### Arming and state machine

The bridge is disarmed at boot and can only be armed from the Inverter portal.
An arm request is accepted only when **all** of the following are true:

* no fault is latched;
* the input rail reads inside 11–14 V (otherwise the arm is refused);
* the ADS7128 answered a probe made at **400 kHz**;
* a full weighted sweep of all five ADS7128 channels has completed, and every
  channel is fresh (less than 50 ms old);
* both TMP235 readings are inside their −40…150 °C specified range.

Otherwise the request is refused with `ARM refused: fault latched`,
`ARM refused: no sensors`, `ARM refused: I2C not at 400 kHz` or
`ARM refused: sensors not ready` or `ARM refused: input rail out of 11-14 V`.
Arming is **idempotent**: a second arm request
does not reset the state machine, so it cannot stop a running bridge.

Once armed it sequences automatically:

`POWER_IDLE` → (sensors valid and input rail in range) → `POWER_WAIT_BUS`
→ (21.75 V bus within ±0.90 V of 21.75 V) → `POWER_STARTING` (soft-start ramp
over 1.5 s, raising m from 0 to 0.30) → `POWER_RUNNING` (closed-loop regulation
toward 120.0 V RMS at the outlet).

While running, the bus must stay within 21.75 ± 1.5 V; up to three consecutive
out-of-range control ticks (30 ms) are tolerated before the fault latches, so a
single load-dump excursion does not stop the inverter. The grace counter, the
input-rail hysteresis and every debounce counter are file-scope state cleared by
`startBridge()` and `stopBridge()`; a new run never inherits them from the
previous one.

While disarmed and not faulted, the ADS7128 probe is retried every 500 ms — the
400 kHz probe first, every time — so a recovered chip is picked up without a
reset. Clearing a fault forces a fresh 400 kHz probe and a full five-channel
sweep, and clears the clock flag so the high-speed bus has to be proved again
before the next arm.

Disarming at any time parks the gates low and returns to `POWER_IDLE`. Disarm is
idempotent too. A fault always clears the armed flag before it parks, so `FAULT`
and `ARMED=Y` can never both be true. Sleep mode, BOOTSEL entry, the 16 s menu
timeout and leaving the Inverter portal all disarm first.

#### Protections

Evaluated only while armed; the bridge is inhibited (not faulted) when sensors are
absent:

* Over-current: input current ≥ 20 A for 2 consecutive control ticks (20 ms),
  so a single noisy INA240 sample cannot stop the inverter (warning at 15 A).
* Over-temperature: either TMP235 ≥ 85 °C (warning at 60 °C).
* Input rail out of 11–14 V (±0.2 V hysteresis, debounced 30 ms like the
  other trips). The hysteresis memory starts in the wide window: a strict
  first sample at exactly 11.0 V plus a millivolt of noise would coin-flip an
  instant fault on a good battery, while a truly bad rail fails even the wide
  window — so the wide start loses nothing.
* Bus voltage out of 21.75 ± 1.5 V while running (the start window is ±0.90 V).
* Output over-voltage: estimated outlet > 126.0 V RMS (120.0 V × 1.05), debounced
  over 2 consecutive control ticks (20 ms) — one tick rejects a single fit
  outlier, twenty milliseconds still keeps the drive inside the receptacle
  rating. The hard ceiling is `OUTLET_V_MAX` = 125 V, the NEMA 5-15R
  receptacle rating; this trip is the backstop above it.
* Output out of reach: the regulator is pinned at the PWM clamp
  (`PWM_M_MAX` = 0.959733) and the estimated outlet is still more than 0.5 V
  below the target, for 20 consecutive control ticks (200 ms). It is
  evaluated before the load limit below, and neither the overvoltage check
  (which needs the estimate above 126 V, the opposite of below-target) nor
  the bridge-missing check (which needs the measured drive far below the
  commanded drive, where this needs it tracking the clamp) can mask it; it
  is debounced because the integrator needs about 100 ms to walk into the
  clamp after a load step. Only
  the PWM clamp counts: the drive ceiling also carries the `OUTLET_V_MAX`
  receptacle limit, which binds at low load, and sitting at that ceiling is
  correct behaviour rather than a failure.
* Load too heavy: estimated output power > 150 W for 50 consecutive control
  ticks (500 ms). The VPT24-6670 is 160 VA, and above that limit the outlet is
  asking for more than the winding can pass continuously. The estimate runs
  about 12 % high, so this is roughly 135 W at the outlet — 85 % of the 115 V
  winding's current rating and 70 % of the 12 V winding's. It is only
  evaluated while the bridge is running, never during the soft-start ramp.
* Output missing: a proportional plausibility check — expected output =
  m·vBus·`A_DROOP`/√2; if the expected value exceeds 15 V and the measured
  output stays below 60 % of it for 5 consecutive 100 Hz ticks, the fault
  `bridge output missing` latches. This monitors the bridge midpoints, not the
  outlet: an open transformer secondary is invisible to these dividers.
* DC offset on the primary > 0.5 V for 2 consecutive least-squares estimates (a new estimate about every millisecond; see Measurements). The trip is evaluated on **every** new fit in `updateRmsAndDc()`, not on the 100 Hz control tick, because it has to react inside the saturation window. The transformer's flux budget puts it at its saturation knee
  11–13 ms after a 1 V offset appears — a flux budget of about 0.011 V·s, so
  the knee for an offset X is 11/X ms. The DC estimate is the mean of a
  one-mains-cycle window **measured in time** (16.667 ms; that window is what
  rejects the 60 Hz fundamental), and after a step of X it only reaches the
  0.5 V threshold once a fraction 0.5/X of the window has been filled with the
  offset, so the detection takes roughly 16.667·(0.5/X) ms plus a few estimate intervals.
The table gives the measured figures; the simple formula is only an
approximation, because the fit weights the samples:

  | offset | detection | saturation |
  | --- | --- | --- |
  | 0.5 V | 15.7 ms | 22.0 ms |
  | 1.0 V | 9.0 ms | 11.0 ms |
  | 2.0 V | 3.2 ms | 5.5 ms |

  These are host-model figures from the simulator, which runs the firmware's own
  sweep with an I2C timing model. The bench must confirm the real figures.

  A 1 V step at full modulation therefore trips at about 9.0 ms against an
  11 ms knee -- roughly 2 ms of margin in the host model, checked in
  `tests/test_v19_protection_latency.py` against an independent fit, not the
  sketch's own code. A 5 V or 10 V step saturates in 2.2 ms or 1.1 ms, faster
  than any windowed 60 Hz estimate can resolve; those are supervisory-only,
  and the hardware has to survive them. The same applies to an offset already
  present at start-up: it is caught during the soft start, where the
  modulation (and the AC flux) is still low and the offset has more flux
  margin -- not at full modulation.

  The fit's DC noise is about 0.1 V at full modulation (the 20 kHz carrier
  aliases into the 339 Hz midpoint filters), so the 0.5 V threshold sits
  about 5x above the noise, and two consecutive estimates make a false trip
  unlikely without slowing the resolvable cases.

  The window is inherently least-squares, not a mean: AIN2 and AIN3 are
  sampled at different instants and held until the next sweep, so differencing
  the held values gives a staircase whose 60 Hz content aliases against the
  sweep. The old count-based window made this concrete: counting 128 samples
  ran 21-26 ms, not one mains period, and the uncancelled fundamental put up
  to 4 V of false DC into the estimate and stopped the inverter at m ~ 0.11
  during soft start. A plain mean over the same samples cannot be fixed by
  counting in time instead -- it still sees the staircase. The per-channel fit
  removes the fundamental exactly, on any span of 12.5 ms or more, because
  each sample carries the carrier phase its conversion started at.
* Sensor stale: checked before the rail/bus comparisons so a dead bus is attributed to the sensor; while driving, any ADS7128 channel older than 50 ms faults the bridge. A chip whose ADC has stopped but whose I2C still answers is caught a second way: three identical 12-bit codes in a row on either midpoint while the bridge is switching means the data is dead (a healthy switching bridge moves the code by more than a hundred LSB between consecutive samples, even at the peak of the 60 Hz swing). The DC-offset trip stands down while a repeat is pending, so the stale samples cannot misname the fault as a DC offset first. A chip that stops converting without NACKing would otherwise leave the protections comparing frozen data.
* No drive measurement: the fit is allowed to be invalid for the milliseconds it takes to fill at the start of a run, but a run that never regains one faults after 500 ms -- without it the DC, over-voltage and bridge-plausibility checks are all blind at once.
* Temperature sensor invalid: either TMP235 outside its -40...150 C specified range (an open or shorted sensor, not a temperature), debounced 30 ms. Over-temperature itself (>= 85 C on either sensor) is debounced 30 ms too; a converter glitch is shorter than that, a real thermal event is far longer.
* ADS7128 communication failure while armed (NACK or read error; every failure path goes through one `adsFail()` so the presence flag, the 400 kHz flag and the sweep state can never drift apart).

Over-current and over-temperature faults are latched and require an MCU reset.
Any other fault can be cleared from the Inverter portal (double tap or hold),
which disarms the bridge and forces a fresh ADS7128 probe plus a full sweep —
recovery does not need an MCU reset.

#### Measurements

The ADS7128 (7-bit address `0x10`, ADDR open) is scanned on a 15-step weighted
schedule: 12 of the 15 conversions are the two bridge midpoints (AIN2/AIN3),
and the slow channels (AIN0 input rail, AIN1 21.75 V bus, AIN4 INA240 current
at 0.1 V/A) are each read once per sequence. In the host I2C model (400 kHz,
the firmware's own transaction sequence) a conversion costs about 0.84 ms, so a
sequence takes about 12.6 ms, each slow channel is sampled about every 12.6 ms,
and each midpoint about every 2.1 ms (about every millisecond combined, since
the two midpoints alternate). These are model numbers, not silicon
measurements; the bench must measure them. A plain five-channel round-robin
gave the midpoints only 270 Hz and made the DC estimate need a full cycle of
held values; the weighted sweep gives each midpoint a fresh sample every ~2 ms.
The I2C
timeout is capped at 2 ms so a stuck bus cannot block the loop for the default
1000 ms.

Each midpoint channel keeps its own 32-entry ring of `(time, volts, sin, cos)`
samples, with the 60 Hz carrier phase captured when the conversion *started*
(the conversion is ~2 us but the I2C frame around it is hundreds of
microseconds). `updateRmsAndDc()` fits each ring separately to
`v = DC + a*sin + b*cos` over the last 16.667 ms and differences the two DC
intercepts (the DC estimate) and the two fundamentals (the drive RMS). The fit
needs at least 4 samples spanning at least 12.5 ms of phase, and its normal
matrix must be well conditioned; otherwise it reports invalid and the run
counts toward the 500 ms "no drive measurement" fault. A fit older than 5 ms
with no new sample is dropped rather than trusted. Both rings are cleared when
the bridge starts and when it stops, so a run never inherits the tail of the
previous one.

The same 16.667 ms window is what rejects the 60 Hz fundamental from the DC
estimate -- but by least squares, not by averaging: a one-cycle mean only
cancels a sinusoid sampled uniformly, while the held midpoint staircase
aliases against the sweep and can leave volts of false DC in the mean. The fit
removes the fundamental exactly on any span of 12.5 ms or more, which is why
the window cannot simply be shortened to make the DC trip faster.

A full weighted sweep completes only when every channel has been seen at least
once (`adsSeenMask`); `sensorsOk` additionally requires the 400 kHz bus, every
channel fresh within 50 ms, and both temperatures plausible. While driving, the
same freshness is re-checked every control tick and a stale channel faults.

The driver clears `OSR_DONE` with a **plain register write**, not with the
"set bit" opcode: `OSR_DONE` is a write-1-to-clear bit (datasheet Table 12),
and the set-bit opcode is documented to *set* bits (datasheet §8.5.2.2). If
`OSR_DONE` does not assert within 200 µs -- which is a hundred times the
conversion time -- the driver does not fault: it reads the result, which is
long since ready, and prints `WARN: ADS7128 OSR_DONE did not assert` once.
This matters because with the averaging filter disabled (`OSR_CFG` = 0, the
reset value, which the datasheet defines as "No averaging") the bit is
documented as the *averaging* status and may never assert at all. A genuinely
dead chip is still caught: the result read fails and raises `ADS7128 read`.

Dividers: AIN0 x0.2151, AIN1 x0.1282, AIN2/3 x0.1282. `R27`, `R29` and `R31`
are all 68 kΩ against 10 kΩ bottoms, so every one of them has a 25.74 V full
scale; the bus sits at 2.79 V, well below the 3.3 V clip.

The drive RMS is measured from the bridge midpoint differential and the outlet
is **estimated**, not measured: `V_out_est = A_DROOP x V_drive_rms -
B_DROOP x I_sec_est` with `A_DROOP` = 8.8735 and `B_DROOP` = 5.666 ohm -- the
transformer's catalogue values, used as they come, with no per-unit fitting.
See "How the outlet is regulated" above for the ±3 % bound. Regulation gain:
0.0003 per 10 ms tick per volt of error, with the amplitude step slew-limited
to 0.010 per tick. The drive the target needs at a given load is
`m = (V_target + B_DROOP x I_sec) x sqrt(2) / (A_DROOP x V_bus)`: 120 V needs
m = 0.879 at no load, 0.897 at 50 W and 0.914 at 100 W on a nominal 21.75 V
bus, and 0.941 at 100 W at the bottom of the bus band -- about 2 % of headroom
against the 0.959733 clamp, and only for a transformer that regulates 8 %.

> [!WARNING]
> The output-voltage, output-missing and DC-offset checks are **supervisory**, not
> fast protection: the ADS7128 sweep is millisecond-scale and the control loop runs at
> 100 Hz. Final protection against fast faults is the LM5122's hardware current
> limit. Do not rely on this firmware as the sole protection.

#### Watchdog

A hardware watchdog of 500 ms is enabled in `powerBegin()` and fed from
`powerService()` every loop, as well as during the boot LED sweep (after each
18 ms step and after the 70 ms closing delay). On reset the UCC27735's input
pull-downs bring the gates low.

#### Serial protocol

`Serial` at 115200 baud. State changes are printed as `STATE: <name>`; a
one-second report prints

```
<STATE> ARMED=<Y/N> SENS=<Y/N> MODE=.. I2C=.. VIN=.. VBUS=.. VOUT=.. VPRI=.. VDC=.. RMSV=.. POUT=.. ISEC=.. UNSUP=.. IIN=.. Tctrl=.. Tinv=.. m=.. MMAX=.. MREQ=.. DLIM=.. SAT=.. CLAMP=..
```

with the modulation index at three decimals. The wire tokens are `I2C=` (the bus the ADS7128 answered on: 400, 100, or 0), `VDC=` (the differential DC intercept in volts), `RMSV=` (1 while the fit is valid, 0 otherwise); `VOUT` is the droop-compensated
**estimate** of the outlet, `VPRI` the measured 12 V drive winding RMS, `POUT`
the estimated output power, `ISEC` the estimated 115 V winding current,
`UNSUP` the "drive cannot support the load" flag, `MMAX` the PWM clamp,
`MREQ` the modulation the target needs at the present load and bus, `DLIM`
the ceiling the regulator is held to, `SAT` the "regulator pinned at that
ceiling" flag and `CLAMP` the "that ceiling is the PWM clamp" flag. The last
six are what the bring-up procedure reads to see how much modulation margin is
left. Faults print `FAULT: <reason>`, warnings `WARN: input current` /
`WARN: temperature` / `WARN: ADS7128 OSR_DONE did not assert` / `WARN: ADS7128 only at 100 kHz; arming disabled` (current and temperature warnings re-print after a stop or fault clear while the condition persists), and arming prints
`ARMED` / `DISARMED` (or `ARM refused: fault latched` / `ARM refused: no
sensors` / `ARM refused: I2C not at 400 kHz` / `ARM refused: sensors not ready` / `ARM refused: no battery (5V mode)` / `ARM refused: input rail out of 11-14 V`).
A cleared fault prints `Fault cleared (disarmed)`; clearing an
over-fault is refused with `CLEAR ignored: over-fault latched`.

### V1.9 Overall

V1.9 combines the complete five-LED Portal Interface Engine with the Lamoka1/2
power stage:

* Five individually controlled WS2812B RGB LEDs
* Single multi-gesture button interface (five gestures)
* Persistent menu cursors and a mode-dependent home menu (three items in
  inverter mode, five in 5V mode)
* Status display plus the V1.8 toy modes in 5V mode; Inverter portal instead
  in inverter mode; no games anywhere
* Adjustable brightness and animation speed, an information interface
* Automatic menu timeout, software shutdown and wake-up, USB BOOTSEL entry
* An Inverter portal for arming, status display and fault clearing
* A firmware-generated H-bridge PWM with 500 ns dead time on both edges, the sine
  advanced by a 20 kHz carrier-wrap ISR on slice 0 with a 64-point interpolated
  table
* Autonomous boost start-up sequencing, closed-loop regulation toward a
  120.0 V RMS target at the outlet, from a droop-compensated estimate of the
  outlet rather than a direct measurement
* Over-current, input-rail, bus-voltage, over-temperature and temperature-sensor, output over-voltage /
  output-missing, output-out-of-reach, load-too-heavy, DC-offset, sensor-stale and
  no-drive-measurement, and ADS7128 communication protection; non-over faults clear from the portal with an
  automatic ADS re-probe and full sweep
* A 500 ms hardware watchdog

The power stage has still never been energised. The 120 V RMS target is
regulated from an **estimate** of the outlet: the board has no isolated output
sense, so nothing in the firmware can see what the 115 V winding is doing. The
transformer model alone holds the outlet within about 3 % of 120.0 V RMS
without per-unit calibration, but the measurement chain adds a few percent, so
treat the as-built unit as about ±8 % until it is calibrated against a meter. A
meter is the only way to know the real outlet voltage.

What this prototype does not have: no reverse-polarity protection, the
board's logic ground is not protective earth
— `J5` pins 3/4 are on their own `/PE` net and the harness has to bond that
to a chassis PE stud (Class I) or the unit has to be marked double-insulated
(Class II) — the TMP235s sense board temperature rather than
FET junction temperature, and there are no gate-source bleed resistors. A
direct isolated output sense (AMC1311B) comes with a later Lamoka1 model.
Treat every claim in this document, including the dead-time and protection logic
above, as something to re-derive from the KiCad source and verify on a scope
before the board is powered. See `AGENTS.md` ("The power stage") for the open
findings and bench requirements.