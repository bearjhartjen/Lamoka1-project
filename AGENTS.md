# AGENTS.md — working notes for AI assistants

This file tells an AI coding assistant how work is done in the Lamoka1 repository.
It describes conventions and known engineering status. It is not a specification;
the KiCad sources and the firmware are the specification.

For how AI is used in this project and the standards its output is held to, see
the "Usage of AI in this repository" section of [`README.md`](README.md#usage-of-ai-in-this-repository).
In short: AI output is not treated as inherently correct. Human review, testing,
and engineering judgment decide what ships.

## What this project is

The Lamoka1 is an open-source 11–14 V DC to 120 V AC inverter, with a charging
section planned but not yet implemented. The `Lamoka1/2` is the inverter-only
variant committed to the Owego Rotary craft fair (Elks Lodge, Owego NY,
November 14). The charging section is deliberately out of scope until after
that date.

Two boards exist:

| Board | State | Location |
| --- | --- | --- |
| **Lamoka1** (main, power + control) | Designed, never energized | `HARDWARE/Kicadfiles/Lamoka1/` |
| **Lamoka1-DEV-Brain** (control only) | Built, assembled, 72 h burn-in passed | `Lamoka1-DEV-Brain/` |

The DEV-Brain exists to develop control-side firmware before the power stage is
brought up. Treat it as the reference for anything the firmware touches.

## Repository map

```
HARDWARE/
  COMPONENTS.md            plain-English MPN list (reference only, NOT the BOM)
  Design_Notes.md          design rationale
  Kicadfiles/Lamoka1/      main board — KiCad 10 project
    Lamoka1.kicad_sch        root sheet (hierarchical)
    Control-section.kicad_sch
    Inversion-section.kicad_sch
    User-Interface-Section.kicad_sch
    Lamoka1.kicad_pcb        PCB layout
    Lamoka1.kicad_pro        netclasses, DRC severities, ERC config
    Lamoka1_Parts.kicad_sym  project-local symbols
    Lamoka1_MOSFET.kicad_sym project-local MOSFET symbols
Lamoka1-DEV-Brain/
  Kicad files/             control-board KiCad project
  Lamoka1-DEV-Brain fab files/   BOM, pick-and-place, Gerbers, PCBWay assembly file
SOFTWARE/
  Operation.md             intended power-up / boot-sequence behavior
  Portal Interface Engine/ the firmware (see below)
  README.md                PIE overview and board compatibility
Lamoka1-DEV-Brain/Readme.md
Images/                    screenshots and renderings referenced by the READMEs
```

`HARDWARE/Kicadfiles/` holds KiCad design files only. Generated or printable
outputs (STL, renders, throwaway exports) belong outside the repo.

## Firmware

The Portal Interface Engine (PIE) lives in `SOFTWARE/Portal Interface Engine/`
as nine Arduino sketches, `V1.ino` through `V1.8.ino`.

**The firmware is user-interface firmware only.** It drives an addressable
WS2812B LED array (`NUM_LEDS = 10`) and the single button through a UI state
machine (status ball, fireplace, lightsaber, rainbow, a "Fireball" game,
settings, sleep, USB BOOTSEL). It
contains no power-conversion code: across all nine sketches there are no
references to PWM, dead-time, gate drivers, the LM5122 boost converter, the
H-bridge, the ADS7128 monitor, the INA240 current sense, the TMP235 temperature
sensors, or I²C. Power-stage control firmware does not exist yet.

Target: RP2354A (Raspberry Pi), built with the arduino-pico core, using
`FastLED` for LED output and `pico/bootrom.h` for `reset_usb_boot()`. This is
**not** an ESP32 target — do not reach for ESP32-specific APIs such as
`ledcSetup()`. `analogWrite()` is not an appropriate PWM choice on this core
either; RP2354A hardware PWM uses the SDK's `pwm_*` API.

### Versioning convention

Each PIE revision is a **new file**, not an in-place edit. `V1.8.ino` is the
current head. It targets the Lamoka1-DEV-Brain and does **not** run on the main
Lamoka1/2 board unmodified, so it is not main-board firmware. Every revision from
`V1.ino` through `V1.8.ino` is DEV-Brain firmware.

- Start new work by copying the current head to the next version number.
- Do not edit `V1.0`–`V1.7`; they are history and compatibility references.
- Do not "tidy up" an older sketch to match a newer one. The differences between
  versions are the record of what changed.

### Firmware ↔ documentation contract

`SOFTWARE/Portal Interface Engine/PIE-v1.8-information.md` is a behavioural
description of `V1.8.ino` written for readers of the firmware. It documents the
button gestures, the UI states, the brightness and speed tables, the entropy
service, timings, and the main loop.

If you change firmware behaviour, update that document in the same change. A
behaviour change with a stale description is a defect.

`SOFTWARE/README.md` records which PIE versions are compatible with which board.
Correct it if compatibility changes.

### V1.8 pin contract

Taken from `V1.8.ino` (`#define` block near the top):

| Pin | Use |
| --- | --- |
| GPIO 11 | `LED_PIN` — WS2812B data |
| GPIO 0 | `BUTTON_PIN` — user button, `INPUT_PULLUP` |
| GPIO 5 | `COLOR_ENTROPY_PIN` — extra digital entropy input |
| GPIO 26 | `ADC0` — entropy source |
| GPIO 27 | `ADC1` — entropy source |
| GPIO 28 | `ADC2` — entropy source |
| GPIO 29 | `ADC3` — entropy source |

That list is the **DEV-Brain** pin map. Only two of those seven pins mean the
same thing on the main board. Verified against the main board netlist
(`Control-section.kicad_sch`, MCU U1):

| V1.8 pin | Main Lamoka1/2 |
| --- | --- |
| GPIO 0 — button | button (matches) |
| GPIO 11 — LED data | LED data (matches) |
| GPIO 5 — digital entropy input | not connected |
| GPIO 26 — ADC entropy input | `TEMP_CTRL`, TMP235 output |
| GPIO 27 — ADC entropy input | `TEMP_INV`, TMP235 output |
| GPIO 28 — ADC entropy input | not connected |
| GPIO 29 — ADC entropy input | not connected |

`NUM_LEDS` is `10`, matching the DEV-Brain's ten LEDs (D1–D10). The main board
fits five (D1–D5), so a sketch written against the DEV-Brain addresses LEDs that
are not there. The menu, the brightness and speed bars, the binary score display,
and the Fireball game all index across the full array, so this appears as a
truncated interface rather than a uniformly dimmer one.

Porting to the main board therefore means changing `NUM_LEDS`, finding a real
entropy source in place of GPIO5 and ADC2/ADC3, and moving off GPIO26/27 so the
temperature sensors stay readable. Nothing has done this yet.

On the DEV-Brain, GPIO 5 and GPIO 6 are unused, so the fan has no PWM speed
control and no tach feedback — it runs whenever the 5 V rail is up. The two
TMP235 sensors are monitored separately from fan control.

## Components and part numbers

`HARDWARE/COMPONENTS.md` is the plain-English component reference, and the
starting point for any question about what part is fitted. Each entry carries the
schematic reference designator so it can be checked against the design directly.
The schematic remains authoritative for connectivity, and the KiCad BOM remains
authoritative for what actually gets ordered.

**Unresolved discrepancy — boost controller part number.** The boost controller
appears as `LM5122QMHX-NOPB` in `README.md`, `SOFTWARE/Operation.md`, and
`HARDWARE/COMPONENTS.md`, but schematic U4 carries `LM5122MH/NOPB`. Both are the
same HTSSOP-20 (PWP) package, but they are different orderable part numbers, so
this is a purchasing question rather than a layout one. Make all four agree
before ordering.

**Transformer.** The fitted part is a `VPT24-4170`, rated **100 VA**, mounted
off-board and wired to the board at J1. Its winding configuration — which J1 pin
pair is which winding, and whether the secondaries are wired in series or in
parallel — is not recorded anywhere in the repository. Establish it before the
power stage is energized.

**Manufacturer data is sparse.** The schematic carries a `Manufacturer` field on
only one component (the INA240A2DR). Everywhere else the manufacturer listed in
`COMPONENTS.md` is inferred from the part number, and a generated BOM will have a
blank manufacturer on most lines. Expect to supply manufacturer data at order
time.

## KiCad work

The projects are KiCad 10. Symbols and footprints are embedded in the `.kicad_sch`
and `.kicad_pcb` files, so they open without external libraries. 3D models are
not in the repo (`.gitignore` excludes them); expect missing 3D bodies.

**KiCad is frequently open on this machine.** A `.lck` file in a project
directory means the GUI has it open. Editing `.kicad_sch`, `.kicad_pcb`,
`.kicad_pro`, or the `*-table` files from the command line while the GUI holds
them can be silently overwritten when the GUI saves. Close KiCad first, or work
on a copy and let a human apply it.

Also:

- Prefer `kicad-cli` for read-only inspection (`sch erc`, `pcb drc`, netlist
  export) and for verification after a change.
- **Verify by netlist, not by eyeball.** A change that preserves the netlist is
  provably connectivity-neutral: export and diff pad connectivity before and
  after. This is how past electronic-rule-check cleanups were shown to be safe.
- `.bak` files and `Lamoka1-backups/` are recovery points. Do not edit, delete,
  or "clean up" them.

### Gotchas learned the hard way

- **Cached `lib_symbols` need the library prefix on the outer symbol only.**
  `power:PWR_FLAG` is correct; `PWR_FLAG_0_0` for the sub-unit is correct;
  prefixing both makes the file fail to load.
- **Do not edit pin blocks with regex.** A pattern that looks anchored will
  over-match and delete neighbouring pins. Parse the s-expression and match on
  position plus pin number.
- **`kicad-cli sch erc --format json` reports positions in units of
  hundredths of an inch, not millimetres.**
- ERC cannot model resistor-divider-driven inputs. Two `pin_not_driven` errors
  on the buck converter's feedback and VOS pins are expected and benign. Project
  `erc_exclusions` in `.kicad_pro` were attempted in several shapes and silently
  ignored by KiCad 10.0.6; use the GUI dialog instead.

### Findings that were investigated and closed

These have been raised before and disproven. Do not re-report them as defects
without new evidence:

- **USB-C CC pull-downs.** `RC0603FR-075K1L` (5.1 kΩ) is correct in both the
  schematic instance and the submitted BOM. The footprint library ID is named
  after a different reference designator's library and is shared by design — not
  an MPN mismatch.
- **H-bridge MOSFET gate/drain wiring.** The schematic uses a project-specific
  MOSFET symbol whose pins match the CSD19536KCS datasheet pinout (1 = G,
  2 = D, 3 = S), and the PCB pads carry the correct gate, drain, and switch-node
  nets on all four devices.
- **LM5122 PowerPAD.** The footprint is properly implemented.
- **LM5122 buck feedback reference.** The LM5122 feedback reference is 1.2 V, not
  0.8 V. The 17 V rail calculation is correct as designed. (The TPS62160 buck
  *does* use 0.8 V — the two differ, and that is not an error.)

## The power stage

The main board's power stage has never been energized. Treat every claim about it,
including claims in this file and in the project documentation, as something to
re-derive from the KiCad source before acting.

Two rules:

1. **Do not energize the power stage** without an explicit human decision.
2. **Do not fabricate a power-stage board** without a fresh verification pass.
   Earlier audits reported issues here that did not survive re-checking. One
   "boost current-sense sizing" finding concluded the converter was current
   starved, by dividing the LM5122 current-limit threshold by the current-sense
   amplifier gain. The datasheet defines that threshold directly across the sense
   resistor — `IPEAK(CL) = 75 mV / RS` (equation 13), and the worked design
   example picks `RS = 75 mV / IPEAK(CL)`. The gain of 10 belongs to the control
   loop's sense path, not to the current-limit comparator. Re-derive from
   datasheets rather than trusting a summary.

Recorded in the project history are a number of genuine findings, plus some
retractions. Because the retractions matter as much as the findings, verify
against `Lamoka1.kicad_pcb` and the schematics rather than against notes.

## Documentation

The prose documentation lags the design in places. `HARDWARE/COMPONENTS.md`
carries its own warning that it is not the BOM.

When documentation and the KiCad source disagree, the KiCad source is correct.
Where you find a discrepancy, fix the prose rather than the design, and say so.

`SOFTWARE/Operation.md` describes the *intended* boot sequence, including
current-waveform monitoring that no firmware implements yet. Read it as design
intent, not as a description of working behaviour.

## Repository practice

- Default branch is `Lamoka1`.
- Do not run `git commit` or `git push`. Editing and creating files, and
  `git add`, are fine; committing and pushing are the maintainer's.
- `.gitattributes` encodes deliberate GitHub Linguist intent, including a note
  explaining a past mistake with `linguist-language=KiCad`. The language bar
  depends on it being committed and pushed. Read the comments before changing it.
- Write Arduino sketches in the existing style: C++ with `#define` pin
  constants, one file per revision.
