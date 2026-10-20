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
* **Waiting for bus** (armed, 21.75 V bus not yet valid) — amber pulse on LED 0.
* **Starting** (soft-start ramp) — green bar growing left to right, tracking the
  bus voltage.
* **Running** — green bar whose height tracks the output regulation (0–120 V RMS).
* **Armed, input rail invalid** — dim amber on LED 0.

Gestures in the Inverter portal:

* **Hold** — if a fault is latched, clear it (over-faults are refused); otherwise
  arm (sensors required) or disarm.
* **Double tap** — clear a non-over fault.
* **Long hold** — return to the home menu (cursor stays on Inverter).
* **Tap** — no action (status display only). The portal is treated as a non-menu
  for gesture synthesis, so a tap here is delayed by the 380 ms double-tap window;
  this is what produces the double-tap fault clear.

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
returns to the Settings menu. It does not clear a fault — that is done from the
Inverter portal.

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

A 2.8 s hold disarms the bridge first, then fills all five LEDs with hot pink for
120 ms and forces the interface back to the default Status mode.

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
4. A short startup sweep across all five LEDs (HSV hues, 18 ms per LED), feeding
   the watchdog after each step, then a 70 ms delay with a final watchdog feed.
5. The interface enters the default Status mode.

The ADS7128 is probed at 400 kHz (falling back to 100 kHz) with the I²C timeout
capped at 2 ms so a stuck bus cannot block the loop; a missing chip is reported
on Serial and disables arming but does not latch a fault.

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

**Both constants are per-unit.** The 8 % regulation is a TYP with no published
maximum, so the catalogue values are only the starting point. The bring-up
procedure fits both of them:

- **No load.** Arm the inverter with nothing connected to the outlet and let it
  settle. Read the drive winding RMS from the serial report (`VPRI=`) and
  measure the open-circuit outlet with a DMM at the receptacle (`V_out0`).
  Then `A_DROOP = V_out0 / V_drive_rms`. This measures the no-load ratio
  directly, so it does not depend on the catalogue figure at all.
- **Two load points.** Connect a known resistive load, say 50 W and then 100 W.
  For each, measure the outlet voltage (`V_out1`, `V_out2`) with the DMM and
  the secondary current (`I_sec1`, `I_sec2`) with a clamp meter on the 115 V
  winding, and record the drive RMS (`V_drive_rms1`, `V_drive_rms2`) from the
  report. With `x_i = A_DROOP × V_drive_rms_i` the two load points give
  `B_DROOP = (x1 − x2)/(I_sec1 − I_sec2)` — the slope of `V_out` against
  `I_sec`, not against the load current on the 12 V side.
- Enter `A_DROOP` and `B_DROOP` in `V1.9.ino` and rebuild.

A DMM reads to about 0.5 %, and the two load points span most of the droop, so
`A_DROOP` comes out to about 0.5 % and `B_DROOP` to about 2 %. That puts the
estimated outlet within roughly 1 % of the measured one across the load range —
which is the accuracy the 120 V target is actually held to, because there is no
isolated output measurement for the loop to close on. The estimate of the output
*power* is about 10 % high (`ETA_BUCK_HBRIDGE` assumes 0.92 for the whole
chain where the transformer alone is 0.89); that error is proportional to the
load, so it is absorbed exactly by the fitted `B_DROOP`.

A direct isolated output sense (an AMC1311B) is designed but **not in this
revision**. Until it is, the 125 V receptacle ceiling is enforced on the
estimate.

#### PWM dead-time scheme

The H-bridge has no hardware dead-time generator; the UCC27735 has interlock but no
built-in dead time (datasheet §6.3.1), so dead time is entirely firmware's job.
The scheme (checked by the host-side tests `tests/test_v19_pwm.py` — PWM/dead-time
model — and `tests/test_v19_model.py` — numeric/source checks):

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
`PWM_M_MAX ≈ 0.9597`. The host-side model `tests/test_v19_pwm.py` confirms zero
overlap and ≥ 500 ns gaps for both legs across the full modulation range,
including the extremes; `tests/test_v19_model.py` checks the numeric constants
and their sources.

Note: GPIO 2 and GPIO 3 share slice 1 and therefore share one counter/phase. This
is not a defect — the scheme does not require a per-channel phase inside a slice;
the second edge's delay comes from the per-channel compare offset (`+2·dt`), which
is independent per channel even on a shared slice.

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
Once armed it sequences automatically:

`POWER_IDLE` → (sensors valid and input rail in range) → `POWER_WAIT_BUS`
→ (21.75 V bus within ±0.90 V of 21.75 V) → `POWER_STARTING` (soft-start ramp
over 1.5 s, raising m from 0 to 0.30) → `POWER_RUNNING` (closed-loop regulation
toward 120.0 V RMS at the outlet).

While running, the bus must stay within 21.75 ± 1.5 V; the first out-of-range
sample gets a one-cycle grace before the fault latches. While disarmed and not
faulted, the ADS7128 probe is retried every 500 ms, so a recovered chip is picked
up without a reset. Clearing a fault forces a fresh probe and a full five-channel
sweep.

Disarming at any time parks the gates low and returns to `POWER_IDLE`. Sleep mode
and BOOTSEL entry disarm first.

#### Protections

Evaluated only while armed; the bridge is inhibited (not faulted) when sensors are
absent:

* Over-current: input current ≥ 20 A (warning at 15 A).
* Over-temperature: either TMP235 ≥ 85 °C (warning at 60 °C).
* Input rail out of 11–14 V (±0.2 V hysteresis).
* Bus voltage out of 21.75 ± 1.5 V while running (the start window is ±0.90 V).
* Output over-voltage: estimated outlet > 126.0 V RMS (120.0 V × 1.05). The hard
  ceiling is `OUTLET_V_MAX` = 125 V, the NEMA 5-15R receptacle rating; this
  trip is the backstop above it.
* Output out of reach: the regulator is pinned at the PWM clamp
  (`PWM_M_MAX` = 0.959733) and the estimated outlet is still more than 0.5 V
  below the target, for 20 consecutive control ticks (200 ms). The check comes
  first so that no other fault masks it, and it is debounced because the
  integrator needs about 100 ms to walk into the clamp after a load step. Only
  the PWM clamp counts: the drive ceiling also carries the `OUTLET_V_MAX`
  receptacle limit, which binds at low load, and sitting at that ceiling is
  correct behaviour rather than a failure.
* Load too heavy: estimated output power > 150 W for 50 consecutive control
  ticks (500 ms). The VPT24-6670 is 160 VA, and above that limit the outlet is
  asking for more than the winding can pass continuously. The estimate runs
  about 10 % high, so this is roughly 135 W at the outlet. It is only
  evaluated while the bridge is running, never during the soft-start ramp.
* Output missing: a proportional plausibility check — expected output =
  m·vBus·`A_DROOP`/√2; if the expected value exceeds 15 V and the measured
  output stays below 60 % of it for 5 consecutive 100 Hz ticks, the fault
  `bridge output missing` latches. This monitors the bridge midpoints, not the
  outlet: an open transformer secondary is invisible to these dividers.
* DC offset on the primary > 1.0 V for 3 consecutive 100 Hz ticks. The
  firmware's comment gives about 90 ms from a 1 V offset to saturation, but that
  figure predates the Rev C flux re-derivation, which puts the knee at roughly
  11–13 ms at the operating point. The threshold is unchanged in this
  revision; re-deriving it is an open item, not something this change silently
  alters.
* ADS7128 communication failure while armed (NACK, conversion timeout, or read
  error).

Over-current and over-temperature faults are latched and require an MCU reset.
Any other fault can be cleared from the Inverter portal (double tap or hold),
which disarms the bridge and forces a fresh ADS7128 probe plus a full sweep —
recovery does not need an MCU reset.

#### Measurements

The ADS7128 (7-bit address `0x10`, ADDR open) is scanned in round-robin across
five channels (AIN0 input rail, AIN1 21.75 V bus, AIN2/AIN3 bridge midpoints,
AIN4 INA240 current at 0.1 V/A), with conversions throttled to one per 200 µs —
each channel updates at roughly 1 kHz. The I²C timeout is capped at 2 ms so a
stuck bus cannot block the loop for the default 1000 ms. The bridge-midpoint
differential is accumulated into a 128-sample ring at a minimum 130 µs cadence
(≈ 16.6 ms, about one mains cycle); the RMS and DC estimates are considered
valid once 96 samples have been collected.

Dividers: AIN0 ×0.2151, AIN1 ×0.1282, AIN2/3 ×0.1282. `R27`, `R29` and `R31`
are all 68 kΩ against 10 kΩ bottoms, so every one of them has a 25.74 V full
scale; the bus sits at 2.79 V, well below the 3.3 V clip. (The old 43.2 kΩ bus
divider clipped at 17.56 V and the old 45.3 kΩ midpoint dividers at 18.25 V —
both below the Rev C bus, which is why they changed.)

The drive RMS is measured from the bridge midpoint differential and the outlet
is **estimated**, not measured: `V_out_est = A_DROOP × V_drive_rms −
B_DROOP × I_sec_est` with `A_DROOP` = 8.8735 and `B_DROOP` = 5.666 Ω, fitted per
unit at bring-up — see "How the outlet is regulated" above. Regulation gain:
0.0003 per 10 ms tick per volt of error, with the amplitude step slew-limited
to 0.010 per tick. The drive the target needs at a given load is
`m = (V_target + B_DROOP × I_sec) × √2 / (A_DROOP × V_bus)`: 120 V needs
m = 0.879 at no load, 0.897 at 50 W and 0.914 at 100 W on a nominal 21.75 V
bus, and 0.941 at 100 W at the bottom of the bus band — about 2 % of headroom
against the 0.959733 clamp, and only if the fitted transformer really regulates
8 %.

> [!WARNING]
> The output-voltage, output-missing and DC-offset checks are **supervisory**, not
> fast protection: each ADS7128 channel updates at roughly 1 kHz and the control
> loop runs at 100 Hz. Final protection against fast faults is the LM5122's
> hardware current limit and the external fusing that the board still needs. Do not
> rely on this firmware as the sole protection.

#### Watchdog

A hardware watchdog of 500 ms is enabled in `powerBegin()` and fed from
`powerService()` every loop, as well as during the boot LED sweep (after each
18 ms step and after the 70 ms closing delay). On reset the UCC27735's input
pull-downs bring the gates low.

#### Serial protocol

`Serial` at 115200 baud. State changes are printed as `STATE: <name>`; a
one-second report prints
`<STATE> ARMED=<Y/N> SENS=<Y/N> VIN=.. VBUS=.. VOUT=.. IIN=.. Tctrl=.. Tinv=.. m=..`
with the modulation index at three decimals. Faults print `FAULT: <reason>`,
warnings `WARN: input current` / `WARN: temperature`, and arming prints `ARMED` /
`DISARMED` (or `ARM refused: fault latched` / `ARM refused: no sensors`). A
cleared fault prints `Fault cleared (disarmed)`; clearing an over-fault is refused
with `CLEAR ignored: over-fault latched`.

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
* A firmware-generated H-bridge PWM with 500 ns dead time on both edges, the sine
  advanced by a 20 kHz carrier-wrap ISR on slice 0 with a 64-point interpolated
  table
* Autonomous boost start-up sequencing, closed-loop regulation toward a
  120.0 V RMS target at the outlet, from a droop-compensated estimate of the
  outlet rather than a direct measurement
* Over-current, over-temperature, bus-voltage, output over-voltage /
  output-missing, output-out-of-reach, load-too-heavy, DC-offset and ADS7128
  communication protection; non-over faults clear from the portal with an
  automatic ADS re-probe and full sweep
* A 500 ms hardware watchdog

The power stage has still never been energised. The 120 V target is regulated
from an **estimate** of the outlet, which has to be calibrated per unit at
bring-up; the board has no isolated output sense. Known open limitations: no
output LC filter on the board (an off-board filter is specified in
`HARDWARE/revB-changes.md`), no input or output fuse and no
reverse-polarity protection, the board's logic ground is not protective earth
— `J5` pins 3/4 are on their own `/PE` net and the harness has to bond that
to a chassis PE stud (Class I) or the unit has to be marked double-insulated
(Class II) — the TMP235s sense board temperature rather than
FET junction temperature, there are no gate-source bleed resistors, and the
direct isolated output sense (AMC1311B) that would replace the estimate is not
in this revision. Treat every claim in this document, including the dead-time
and protection logic above, as something to re-derive from the KiCad source and
verify on a scope before the board is powered. See `AGENTS.md` ("The power
stage") for the open findings and bench requirements.