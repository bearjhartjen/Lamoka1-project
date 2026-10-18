## Lamoka1/2 — PIE Version V1.9

The **Lamoka1/2 PIE Version V1.9** is the first Portal Interface Engine revision that runs
on the main Lamoka1/2 board. It keeps the complete five-LED user interface from V1.8 and
adds the H-bridge power stage: the LM5122 boost is left to run in hardware, while the
RP2354A generates the four gate-driver PWM signals, reads the ADS7128 monitor chain, and
runs the inverter's control and protection loops. V1.9 is **not** DEV-Brain firmware and
does not run unmodified on the Lamoka1-DEV-Brain.

The firmware is written in Arduino C++ using the **FastLED** library for LED output,
the Raspberry Pi Pico SDK (`hardware/pwm`, `hardware/adc`, `hardware/watchdog`) for the
power stage, `Wire` for the ADS7128, and `pico/bootrom.h` for `reset_usb_boot()`.

### Hardware Interface

V1.9 is written for the main Lamoka1/2 board (per `HARDWARE/COMPONENTS.md`, MCU is the
RP2354A made by Raspberry Pi):

* **GPIO 11** — WS2812B LED data (D1–D5, five LEDs)
* **GPIO 0** — User button (B3F-4055), `INPUT_PULLUP`
* **GPIO 8 / 9** — I²C0 SDA/SCL to the ADS7128 (U? monitor MUX), 400 kHz
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

V1.9 is organised around ten UI states:

* **Mode** — normal lighting operation
* **Main Menu**
* **Games Menu**
* **Settings Menu**
* **Brightness**
* **Speed**
* **Info**
* **Inverter** (new in V1.9 — the power-stage portal)
* **In Game**
* **Sleep**

Menus remember their previous cursor positions, so returning to Games or Settings does
not reset the cursor.

### Main Menu

The main menu contains seven positions:

1. **Status**
2. **Fireplace**
3. **Lightsaber**
4. **Rainbow**
5. **Inverter**
6. **Games**
7. **Settings**

Positions 0–4 use the cactus-green menu colour, Games is orange and Settings is blue.
The selected position is displayed in **hot pink** with a breathing brightness
animation. A tap moves forward, a double tap moves backward, a hold selects, and a
long hold returns to the default idle state. With seven items on five LEDs the menu
window scrolls so the cursor is always visible.

### Default Status Mode

The default idle display is a **blue bouncing ball** moving across the five LEDs,
with two trailing LEDs for a short blue trail. The movement rate is affected by the
global speed setting. Status mode is selected automatically at startup and on return
from shutdown, menu timeout, or the appropriate long-hold action.

### Fireplace Mode

Unchanged from V1.8 in behaviour, scaled to five LEDs: a 5-element heat model with
cooling, source injection, neighbour injection, diffusion, rising heat, random flame
licks, high sparks, base glow, noise modulation and flare effects. Colour progresses
dark red → red → orange → yellow → white-hot. A tap flares; a double tap flares
harder and re-targets the source.

### Lightsaber Mode

Unchanged from V1.8, scaled to five LEDs: a coloured blade that extends outward,
with random hue (never the same as the previous launch), saturation, pattern and
instability; a white core blend; an ignition flash on LED 0 for the first 170 ms; tap
to stop/relaunch, double tap to relaunch, hold for a clash effect.

### Rainbow Mode

Unchanged from V1.8, scaled to five LEDs: `fill_rainbow()` across the five LEDs, ten
tap-cycled speeds, double tap to reverse direction, randomised starting hue on entry.

### Inverter Portal (new in V1.9)

The **Inverter** home item is the power-stage portal. It shows the inverter state on
the five LEDs and is the only place the bridge can be armed or disarmed:

* **Faulted** — pulsing red (all five LEDs).
* **No sensors** (ADS7128 not responding) — amber dot on LED 0.
* **Disarmed** — dim blue standby dot on LED 0.
* **Waiting for bus** (armed, 17 V rail not yet valid) — amber pulse on LED 0.
* **Starting** (soft-start ramp) — green bar growing left to right, tracking the
  bus voltage.
* **Running** — green bar whose height tracks the output regulation (0–115 V RMS).
* **Armed, input rail invalid** — dim amber on LED 0.

Gestures in the Inverter portal:

* **Hold** — arm (if sensors are present and no fault is latched) or disarm.
* **Double tap** — clear a (non-latched) fault.
* **Long hold** — return to the home menu (cursor stays on Inverter).
* **Tap** — no action (status display only).

The bridge is **never energised without an explicit arm action**. A valid voltage
reading is not treated as permission to switch. Sleep mode and BOOTSEL entry both
disarm the bridge before acting.

### Fireball Game

Unchanged from V1.8, scaled to five LEDs: the fireball starts at LED 4 and travels
toward LED 0 (the target). Three lives, 115 ms movement interval, 10 ms speed-up per
catch, 30 ms minimum. The score is displayed as a 5-bit binary value (maximum 31) on
the five LEDs. A tap catches when the fireball is on LED 0; a long hold exits to the
Games menu.

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
returns to the Settings menu (or clears a fault if one is latched).

### Entropy and Randomness

V1.9 uses the RP2354A's hardware TRNG (`get_rand_64()`) instead of the analog
channels (which are TMP235 sensors on this board). An `EntropyService` mixes the
hardware value with `micros()` using XOR and shifts, providing 32-bit, 8-bit, ranged
and boolean random values. It is initialised with **300 mixing operations** and
updated every loop iteration.

### Menu Timeout

**16 seconds** of inactivity returns the interface to the default blue Status mode,
resetting the main mode and cursor.

### Very-Long-Hold Function

A 2.8 s hold fills all five LEDs with hot pink and forces the interface back to the
default Status mode.

### Shutdown Mode

Software sleep: LEDs cleared, brightness zero, wait for button release, then wake on
a 1.5 s hold, restoring brightness and Status mode. The bridge is disarmed on entry.

### USB BOOTSEL Mode

Clears the LEDs, waits 40 ms, and calls `reset_usb_boot(0, 0)`. The bridge is
disarmed first.

### Startup Sequence

1. `powerBegin()` parks all four gate-driver inputs low (SIO outputs, LOW) before
   anything else.
2. FastLED is initialised for the five WS2812B LEDs; dither disabled; brightness
   applied; LEDs cleared.
3. The entropy and button services are initialised.
4. A short startup sweep across all five LEDs (HSV hues, 18 ms per LED).
5. The interface enters the default Status mode.

The ADS7128 is probed at 400 kHz (falling back to 100 kHz); a missing chip is
reported on Serial and disables arming but does not latch a fault.

### Main Program Loop

On every iteration:

1. `powerService(now)` — watchdog feed, ADS sampling, temperature reads, RMS update,
   control loop, PWM update, state machine, serial reporting.
2. If in sleep mode, run the shutdown update and return.
3. Update the entropy service.
4. Check the menu timeout.
5. Poll the button and dispatch gestures.
6. Render the current UI state and call `FastLED.show()` at the 16 ms frame interval.

### Power Stage

#### Topology

Battery 11–14 V → `Rsense1` → LM5122 boost → 17 V bus → UCC27735 gate drivers →
Q3/Q4/Q5/Q6 H-bridge → 12 V transformer winding → 115 V outlet. The LM5122 runs in
hardware; the MCU only generates PWM.

#### PWM dead-time scheme

The H-bridge has no hardware dead-time generator; the UCC27735 has interlock but no
built-in dead time (datasheet §6.3.1), so dead time is entirely firmware's job.
The scheme (verified by simulation, see below):

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
`2·dt`. Combined with the phase offsets this gives each leg a **500 ns gap on
both edges** for any duty in the allowed band, which is why `|m|` is clamped to
`PWM_M_MAX ≈ 0.9597`. A simulation of the counter waveforms (host-side, Python)
confirmed zero overlap and ≥ 500 ns gaps for both legs across the full modulation
range, including the extremes.

Note: GPIO 2 and GPIO 3 share slice 1 and therefore share one counter/phase. This
is not a defect — the scheme does not require a per-channel phase inside a slice;
the second edge's delay comes from the per-channel compare offset (`+2·dt`), which
is independent per channel even on a shared slice.

#### Arming and state machine

The bridge is disarmed at boot and can only be armed from the Inverter portal.
Once armed it sequences automatically:

`POWER_IDLE` → (sensors valid and input rail in range) → `POWER_WAIT_BUS`
→ (17 V bus within ±0.75 V of 17 V) → `POWER_STARTING` (soft-start ramp over 1.5 s)
→ `POWER_RUNNING` (closed-loop regulation).

Disarming at any time parks the gates low and returns to `POWER_IDLE`. Sleep mode
and BOOTSEL entry disarm first.

#### Protections

Evaluated only while armed; the bridge is inhibited (not faulted) when sensors are
absent:

* Over-current: input current ≥ 20 A (warning at 15 A).
* Over-temperature: either TMP235 ≥ 85 °C (warning at 60 °C).
* Input rail out of 11–14 V (±0.2 V hysteresis).
* Bus voltage out of 17 ± 1.5 V while running.
* Output over-voltage: inferred output > 143.75 V RMS.
* Output missing: ramp > 5 % but output < 11.5 V RMS.
* DC offset on the primary > 2 V.
* ADS7128 communication failure while armed.

Over-current and over-temperature are latched and require an MCU reset (or, for
non-`over-` faults, a clear from the Inverter portal, which also disarms).

#### Measurements

The ADS7128 (7-bit address `0x10`, ADDR open) is scanned in round-robin across
five channels (AIN0 input rail, AIN1 17 V bus, AIN2/A3 bridge midpoints, AIN4
INA240 current at 0.1 V/A). The bridge-midpoint differential is accumulated into a
128-sample ring at a minimum 130 µs cadence (≈ 16.6 ms, about one mains cycle);
the RMS and DC estimates are considered valid once 96 samples have been collected.
Dividers: AIN0 ×0.2151, AIN1 ×0.1879, AIN2/3 ×0.1808. Output RMS is inferred as
`(AIN2 − AIN3) × 9.583` (transformer 115:12). Regulation gain: 0.0003 per 10 ms
tick per volt of error (≈ 2 Hz crossover), tuned for the ≈ 16 ms measurement lag.

> [!WARNING]
> The output-voltage and DC-offset checks are **supervisory**, not fast
> protection: the ADS7128 is sampled in the kHz range and the control loop runs at
> 100 Hz. Final protection against fast faults is the LM5122's hardware current
> limit and the external fusing that the board still needs. Do not rely on this
> firmware as the sole protection.

#### Watchdog

A hardware watchdog of ≈ 200 ms is enabled in `powerBegin()` and fed from
`powerService()` every loop. On reset the UCC27735's input pull-downs bring the
gates low.

#### Serial protocol

`Serial` at 115200 baud. State changes are printed as `STATE: <name>`; a
one-second report prints
`<STATE> ARMED=<Y/N> SENS=<Y/N> VIN=.. VBUS=.. VOUT=.. IIN=.. Tctrl=.. Tinv=.. m=..`.
Faults print `FAULT: <reason>`.

### V1.9 Overall

V1.9 combines the complete five-LED Portal Interface Engine with the Lamoka1/2
power stage:

* Five individually controlled WS2812B RGB LEDs
* Single multi-gesture button interface (five gestures)
* Persistent menu cursors and a seven-item home menu
* Four lighting modes (Status, Fireplace, Lightsaber, Rainbow) plus the Fireball game
* Adjustable brightness and animation speed, an information interface
* Automatic menu timeout, software shutdown and wake-up, USB BOOTSEL entry
* Hardware-derived entropy (RP2350 TRNG)
* An Inverter portal for arming, status display and fault clearing
* A firmware-generated H-bridge PWM with 500 ns dead time on both edges
* Autonomous boost start-up sequencing, closed-loop output regulation
* Over-current, over-temperature, bus-voltage, output and DC-offset protection
* A 200 ms hardware watchdog

The power stage has never been energised. Treat every claim in this document,
including the dead-time and protection logic above, as something to re-derive from
the KiCad source and verify on a scope before the board is powered. See
`AGENTS.md` ("The power stage") for the open findings and bench requirements.