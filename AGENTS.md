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
| **Lamoka1/2** (main, power + control) | Designed, never energized | `HARDWARE/Kicadfiles/Lamoka1-half/` |
| **Lamoka1-DEV-Brain** (control only) | Built, assembled, 72 h burn-in passed | `Lamoka1-DEV-Brain/` |

The DEV-Brain exists to develop control-side firmware before the power stage is
brought up. Treat it as the reference for anything the firmware touches.

**Naming.** The main board is the Lamoka1/2 in prose, and the project as a whole
is the Lamoka1. A slash cannot appear in a path, so every file, directory and
library nickname writes it `Lamoka1-half` — `HARDWARE/Kicadfiles/Lamoka1-half/`,
`Lamoka1-half.kicad_sch`, `Lamoka1-half.pretty`, and footprint references such as
`Lamoka1-half:PWP0020A_N`. The same convention is behind
`HARDWARE/Mechanical/Lamoka1-half.step`. Prose documents keep the slash; the
branch, the repository name and the DEV-Brain keep theirs.

## Repository map

```
HARDWARE/
  COMPONENTS.md            plain-English MPN list (reference only, NOT the BOM)
  Design_Notes.md          design rationale
  Transformer-wiring.md    J1 lead-to-terminal spec (read before wiring)
  Kicadfiles/Lamoka1-half/ main board — KiCad 10 project
    Lamoka1-half.kicad_sch   root sheet (hierarchical)
    Control-section.kicad_sch
    Inversion-section.kicad_sch
    User-Interface-Section.kicad_sch
    Lamoka1-half.kicad_pcb   PCB layout
    Lamoka1-half.kicad_pro   netclasses, DRC severities, ERC config
  Lamoka1-half fab files/  main-board gerbers, drill, BOM, pick-and-place
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
outputs (STL, renders, throwaway exports) belong outside the repo. KiCad
library sources (symbols and footprints) are kept local and are not committed
either — see "Repository practice" below.

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
current DEV-Brain head. `V1.9.ino` is the first revision that runs on the
main Lamoka1/2 board (five LEDs, TMP235 on GPIO 26/27, RP2350 TRNG entropy,
plus the H-bridge power stage). It does not run on the DEV-Brain unmodified, and
V1.8 does not run on the main board unmodified. Every revision from `V1.ino`
through `V1.8.ino` is DEV-Brain firmware.

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

The fan is a main-board part and is wired to the 5 V rail through two solder
pads (`J4` = FAN GND, `J6` = FAN 5V), so it runs whenever 5 V is up: no PWM
speed control, no tach feedback, and GPIO 5 is unused on both boards. The
DEV-Brain has no fan and no TMP235 sensors — its GPIO 5 and GPIO 6 run to header
`J1` pins 5 and 6 and test points `TP10`/`TP11`. The two TMP235 sensors are
monitored separately from fan control.

## Components and part numbers

`HARDWARE/COMPONENTS.md` is the plain-English component reference, and the
starting point for any question about what part is fitted. Each entry carries the
schematic reference designator so it can be checked against the design directly.
The schematic remains authoritative for connectivity, and the KiCad BOM remains
authoritative for what actually gets ordered.

**Boost controller part number — resolved.** The boost controller is
`LM5122MHX/NOPB` (U4): the industrial-grade LM5122 in HTSSOP-20 (PWP), supplied on
tape and reel. Choose it over `LM5122QMHX/NOPB`, which is the AEC-Q100 automotive
grade and costs more for a qualification this design does not need — the package
and land pattern are identical, so this is purely a purchasing choice.

Footprint caution: the `LM5122QMHX-NOPB` footprint library in `fp-lib-table`
(`PWP20_3P0X4P2`) has **no thermal pad at all** — only the 20 lead pads. A
footprint update from that library fails with "pad 21 not found" and breaks the
PowerPAD connection. U4 must stay on `Lamoka1-half:PWP0020A_N`, the local copy
of the corrected footprint.

**Transformer.** The fitted part is a `VPT24-4170`, rated **100 VA**, mounted
off-board and wired to the board at J1. `HARDWARE/Transformer-wiring.md` records
the lead-to-terminal map. Read it before wiring anything: J1's eight poles are
four pairs, one pair per parallel-connected winding, and the lead colours must
**not** be matched to the net names — `/BLUE`, `/BROWN`, `/BLACK` and `/RED` are
lead colours, and this transformer's BLUE/BROWN leads are its 115 V winding,
while the board's bridge sits on the `/BLUE`/`/BROWN` nets. Confirm the map
against the physical unit before the power stage is energized.

**Manufacturer data is sparse.** Every symbol carries a `Manufacturer` field but
it is populated on exactly one: SW1's button (`Aratas (Formerly Omron
Components)`). Everywhere else the manufacturer listed in `COMPONENTS.md` is
inferred from the part number, and a generated BOM will have a blank
manufacturer on most lines. Expect to supply manufacturer data at order time;
`tools/make_fab_bom.py` does it for the PCBWay BOM from a hand-maintained map.

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
- `.bak` files and `Lamoka1-half-backups/` are recovery points. Do not edit, delete,
  or "clean up" them.

### Footprint library `Lamoka1-half.pretty`

Thirteen footprints on the board come from the project-local library
`Lamoka1-half.pretty`, registered in `fp-lib-table` as `${KIPRJMOD}/Lamoka1-half.pretty`.
Like the vendor libraries, the library itself is **kept local, not committed**
(see "Repository practice"): the copies that ship in the repository are the
embedded footprints inside `Lamoka1-half.kicad_pcb`, so a fresh clone opens, plots
and fabricates the board complete, but has no library to update these
footprints from. They are `CP_Elec_8x11.9_Cout1`, `CP_Elec_8x11.9_Cout2`,
`CP_Elec_8x11.9_Cout3`, `CONN8_2606-1108&slash_010-000_WAG`,
`CONN2_2606-1352_WAG`, `CONN3_2604-1103_WAG`, `DQG0008A`, `DO-214AC_VIS`,
`DSG8_1P6X0P9`, `PWP0020A_N`, `IND_PA4332.222NLT_PUL`, `R_0603_1608Metric`
and `SOT-223-3_TabPin2`.

They are copies of the footprints **as they sit on the board**, not the vendor
originals, because the vendor libraries were wrong in ways that would damage the
board if applied:

- `PWP0020A_N` and `DSG8_1P6X0P9` put their pads on the bottom layer of a
  front-layer footprint.
- `DO-214AC_VIS` and `IND_PA4332.222NLT_PUL` contained zero-length courtyard
  lines.
- `DQG0008A` had no paste apertures over the thermal pads. The board adds four
  paste-only pads, a mask opening on pad 9, and drops the vendor's seven `V`
  thermal vias.
- `DSG8_1P6X0P9` had no paste apertures on pads 1–8 at all.

Because the board copy and the working-library copy agree, **"Update Footprints
from Library" is a no-op** for these parts where the library is present, and
`kicad-cli pcb drc` reports no `lib_footprint_mismatch` and no
`lib_footprint_issues`.

If one of these footprints is edited on the board, regenerate its local library
copy to match, or the mismatch warning comes back on this machine. Three rules
govern the conversion:

- A library footprint carries no `(at …)`, `uuid`, `path`, `sheetname`,
  `sheetfile`, `(locked)` or pad `net`.
- **Pad angles are stored in board coordinates.** Subtract the footprint's own
  rotation to get the library angle. `Cout1`–`Cout3`, `CONN2`, `CONN3`,
  `CONN8`, `R_0603_1608Metric`, `IND_PA4332.222NLT_PUL` and `DO-214AC_VIS`
  all depend on this.
- **Zones embedded in a footprint hold absolute board coordinates.** Subtract
  the footprint origin and rotate back by its angle. `DO-214AC_VIS` and
  `IND_PA4332.222NLT_PUL` carry keepout zones and need this.

Do not use "Update from library" to revert a deliberate change; edit the copy in
`Lamoka1-half.pretty` instead — and keep that copy out of the repository.

### Isolation barrier and the October 2026 board revision

`Lamoka1-half.kicad_dru` carries a custom rule that holds the 120 V transformer
secondary (`/BLACK`, `/RED`) 1.5 mm clear of every other net. IPC-2221B table
6-1 column B2 asks for 1.25 mm at 151-300 V working, so 1.5 mm has margin.
Everything else on the board is 17 V or below and stays on the 0.18 mm netclass.
The rule is what pulls the GND pour back from the secondary: **refill the zones
after any copper edit**, or the plotted pour will not obey it.

Before the rule existed the secondary ran as close as 0.93 mm to control-side
copper (three `/GPIO1` pairs) and 1.41 mm to `/GPIO2`. The same revision lifted
the BLACK run 0.6 mm off that corridor on both layers — those segments no longer
sit on the grid their neighbours use — moved the `Rvin1` and `R19` references
off the pads they were clipping, brought the connector references (`J1`, `J2`,
`J4`, `J5`, `J6`) onto silkscreen, and put the "DANGER! HIGH VOLTAGE" legend in
the empty band between `J1`'s pad rows. A copper-to-copper barrier scan now finds
nothing closer than 1.5 mm, and the secondary stays 3.4 mm inside the board
edge. `kicad-cli pcb drc` reports seven warnings, all design intent: four
dangling GND vias and one isolated `1V1` fragment under U1's keepout, and two
`J3` silkscreen segments clipped by the board edge. Adding
`--schematic-parity` reports 17 symbol-versus-footprint warnings, also design
intent (twelve `V` thermal-via pads on U4's footprint, three MPN fields that
live only on symbols, two BOM-exclusion attributes that live only on
footprints); the DEV-Brain, which was fabricated and works, reports 94 of the
same kind. `HARDWARE/Lamoka1-half fab files/README.md` carries the breakdown.

The submission package for the main board lives in `HARDWARE/Lamoka1-half fab files/`
(gerbers, drill file, PCBWay BOM and pick-and-place, plus a README of ordering
notes); its BOM is regenerated with `tools/make_fab_bom.py`.

### Gotchas learned the hard way

- **Cached `lib_symbols` need the library prefix on the outer symbol only.**
  `power:PWR_FLAG` is correct; `PWR_FLAG_0_0` for the sub-unit is correct;
  prefixing both makes the file fail to load.
- **Do not edit pin blocks with regex.** A pattern that looks anchored will
  over-match and delete neighbouring pins. Parse the s-expression and match on
  position plus pin number.
- **`kicad-cli pcb drc` only reports schematic-parity problems when asked.** Run
  it with `--schematic-parity`, as `CONTRIBUTING.md` documents. Without the flag
  it prints "0 Footprint errors" and hides the symbol-versus-footprint
  differences entirely — an earlier pass mistook that for a clean parity check.
- **`kicad-cli sch erc --format json` reports positions in units of
  hundredths of an inch, not millimetres.**
- ERC cannot model resistor-divider-driven inputs, so `pin_not_driven` warnings
  on the buck converter's feedback and VOS pins used to be expected and benign.
  The October 2026 revision reports none: `ERC.rpt` and a live
  `kicad-cli sch erc` both say 0 errors, 0 warnings, so treat a new
  `pin_not_driven` as a real change. Editing `erc_exclusions` in `.kicad_pro` by
  hand was attempted in several shapes and silently ignored by KiCad 10.0.6; use
  the GUI dialog instead. (`erc_exclusions` is currently empty.)

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
against `Lamoka1-half.kicad_pcb` and the schematics rather than against notes.

#### Open findings from the 2026-10-18 reviews

Two electrical reviews read the netlist, `COMPONENTS.md`, `Operation.md` and the
LM5122, UCC27735, INA240, ADS7128, TMP235, AMS1117, TPS62160 and Triad
datasheets. Their findings are recorded here so the next pass re-derives and
decides rather than rediscovers. They are **unverified by measurement and not yet
acted on** — the usual rule applies, re-derive from the source before acting.

**Decisions for the maintainer, before the board is ordered or energised.** Each
of these is a schematic-and-layout change, or a decision to accept present
behaviour. None has been applied.

- **Transformer hook-up (the serious one).** The board's bridge sits on the
  `/BLUE`, `/BROWN` nets and the outlet on `/BLACK`, `/RED`, but the
  VPT24-4170's BLUE/BROWN leads are its 115 V winding and its 12 V winding is
  BLACK/YELLOW/RED/ORANGE. Fitted colour-for-colour, the bridge drives the 115 V
  winding and the outlet gets about 1.9 V instead of ~120 V. The eight-pole
  connector is already wired for the parallel connection of both windings, so
  the fix is a harness spec, not copper: `HARDWARE/Transformer-wiring.md`.
- **`Rsense1` is 2 mΩ, so the boost's cycle-by-cycle limit is 37.5 A** — about
  three times the 12.1 A peak the 100 VA design draws, and above the
  `XAL1580-182MEB` saturation class, so the inductor can saturate before the
  limit trips. The LM5122 procedure wants 4.1–5.6 mΩ, and any replacement has to
  respect the shunt's power rating (2 mΩ at 12 A is already 0.29 W).
- **`C13` is a 10 µF 0402 ceramic on the AMS1117-3.3 output.** The AMS1117
  datasheet asks for 22 µF and names tantalum: the ESR is part of the part's
  frequency compensation, and near-zero-ESR MLCC is the risky condition.
- **The gate drivers have no local supply bypass.** `U5`/`U6` pin 7 (VDD) sits on
  the 17 V rail with the nearest bulk capacitor centimetres away; the datasheet
  asks for about 1 µF at the pin (CVDD ≥ 10 × CBOOT).
- **No fuse anywhere.** Neither the 11–14 V input nor the 120 V output is fused;
  the only series element on `+BATT` is a 2 mΩ measurement shunt.

**Firmware status (V1.9, 2026-10-18).** The first main-board firmware now exists
(`SOFTWARE/Portal Interface Engine/V1.9.ino`): five-LED UI ported from V1.8, RP2350
TRNG entropy, TMP235 reads on GPIO 26/27, explicit arming, PWM dead-time scheme
verified by host-side simulation (500 ns on both edges, both legs, full modulation
range), soft-start ramp, closed-loop regulation, and supervisory protection. It
compiles clean (84.7 kB program, 15.9 kB globals). The power stage is still
**never energised**; bench verification (scope dead-time check, ADC/current/
temperature validation, staged energisation) is outstanding. The audit's minimum
change set (C13, U5/U6 bypass, Rsense1, fusing) is still not applied.

- **Dead time is entirely firmware's job.** The UCC27735 has interlock but no
  built-in dead time (datasheet §6.3.1); no hardware prevents overlap.
- **The bridge must stay gated off until the boost has finished start-up.**
  `Css1` (33 nF) is far below LM5122 Eq. 12 for a pre-loaded 587 µF bus; the
  design relies on the load being absent during soft start.
- **No gate-source bleed resistors on `Q1`–`Q6`.** The drivers hold the gates low
  while powered; a dead driver leaves them floating.
- **The AC output is not measured directly.** The monitor chain is the battery
  rail (`AIN0`), the 17 V rail (`AIN1`), the two bridge midpoints
  (`AIN2`/`AIN3`), load current (`AIN4`, INA240) and two TMP235s. Firmware infers
  the output from the drive winding and the 115:12 ratio.
- **No USB ESD protection** on `USB-5v`, D+/D− or CC.
- **`AIN1` headroom.** The 17 V divider (`R27` 43.2 k / `R28` 10 k) puts 3.195 V
  on a 3.3 V input and clips at 17.7 V; a 68 k top resistor would restore margin.
- **The isolation barrier has no slack.** The closest approach is 1.5005 mm
  against the 1.5 mm rule (IPC-2221B asks 1.25 mm at 151–300 V); any future
  copper edit near `/BLACK`/`/RED` needs the barrier scan re-run.

**Boost-converter observations.**

- **Plug-in inrush.** With a battery connected, the 587 µF bus charges through
  `Rsense1` → `L2` → `Q1`'s body diode with only milliohms of limiting, because
  the converter is unpowered and `Css1` cannot soft-start it. There is no fuse,
  NTC, precharge or hot-swap on `+BATT`.
- **No off switch.** `U4` has no enable pin and the UVLO divider is passive,
  although datasheet §7.3.1 offers the UVLO pin as a remote shutdown; the rail
  sits at ~VIN whenever a battery is attached, with no designed bleeder.
- **`Q1` voltage margin.** A 25 V part on the 17 V switching node at ~495 kHz
  with no snubber, while `Q2` is 30 V. The LM5122 drives both gates directly, as
  its typical application does; the asymmetry is recorded nowhere.
- **Ripple.** 1.8 µH at ~495 kHz gives about 4.4 A pk-pk on a 5 A average
  (≈88 %), so the converter enters DCM below roughly 1.4 A at 14 V input.
- **Derived-but-undocumented values.** Re-derivation puts `Rslope1` at K = 0.857
  (valid) and the `COMP` network at ~2.3 kHz with a 22 kHz right-half-plane zero
  (plausible); `Cres1` is datasheet-consistent. `Css1` is the exception above.
- **Grounding.** `AGND`, `PGND` and the low-side returns are one `GND` net in the
  schematic; the star point is not expressed anywhere (`NT1` is a board net tie).
- **`MODE` strap.** `MODE` is tied to GND: diode emulation with the skip-cycle
  comparator disabled, i.e. pulse skipping at light load. Valid, but the choice
  is recorded nowhere.
- **Net names.** The input rail is `/Inversion-Section/CSP` and the 17 V rail is
  `/Inversion-Section/G4D` — named after unrelated pins, which makes the boost
  and the `.kicad_dru` rules that key on net names harder to audit.
- **Polarisation.** The electrolytic and polymer output capacitors are drawn with
  `Device:C`, whose description reads "Unpolarized capacitor".
- **Thermal.** The FETs' `DQG0008A` footprints carry no thermal vias by design;
  the resulting junction-temperature budget is not written down.
- The remaining DRC leftovers (four dangling GND vias, one isolated `1V1`
  sliver, two `J3` silkscreen clips) are the design intent documented above.

The stale board `Value` fields the same reviews flagged (`D6`, `D11`, `D12`,
`CR1`, `CR2`) were synced to the schematic on 2026-10-18 and no longer apply.

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
- **KiCad library files stay out of the repository.** Component symbol and
  footprint sources (`*.kicad_sym`, `*.kicad_mod`, `*.pretty/`) and 3D models
  are kept local and ignored by the `.gitignore` files; the `.kicad_sch` and
  `.kicad_pcb` files carry embedded copies of everything the design needs.
  Never commit a library file.
- Do not run `git commit` or `git push`. Editing and creating files, and
  `git add`, are fine; committing and pushing are the maintainer's.
- `.gitattributes` encodes deliberate GitHub Linguist intent, including a note
  explaining a past mistake with `linguist-language=KiCad`. The language bar
  depends on it being committed and pushed. Read the comments before changing it.
- Write Arduino sketches in the existing style: C++ with `#define` pin
  constants, one file per revision.
