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

**Naming.** **Lamoka1/2** is a model designation, not a shorthand for "the main
board" or a prose convenience: it names the inverter-only model in the Lamoka1
project, the one committed to the Owego Rotary craft fair. Write it with the
slash, capitalised exactly that way, wherever the model is named — titles,
tables, silkscreen, filenames' surrounding text, and prose. The project as a
whole is the Lamoka1, and the control-board model is the Lamoka1-DEV-Brain.

A slash cannot appear in a filesystem path, so files, directories and library
nicknames use the filesystem alias `Lamoka1-half` —
`HARDWARE/Kicadfiles/Lamoka1-half/`, `Lamoka1-half.kicad_sch`,
`Lamoka1-half.pretty`, `Lamoka1-half:PWP0020A_N`, and
`HARDWARE/Mechanical/Lamoka1-half.step`. That alias is only a path-safe spelling
of the model name; it does not replace the name in documentation. The branch,
the repository name and the DEV-Brain keep their own names.

## Repository map

```
HARDWARE/
  COMPONENTS.md            plain-English MPN list (reference only, NOT the BOM)
  Design_Notes.md          design rationale
  Transformer-wiring.md    J1 lead-to-terminal spec (read before wiring)
  revB-changes.md          Rev B and Rev C change sets (found, and what was applied)
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
as two Arduino sketches, `V1.8.ino` (the DEV-Brain head) and `V1.9.ino` (the
Lamoka1/2 main-board head). Revisions `V1.0` through `V1.7` are no longer in the
tree; git history preserves them if an old behaviour has to be consulted.

**`V1.8.ino` is user-interface firmware only.** It drives an addressable
WS2812B LED array (`NUM_LEDS = 10`) and the single button through a UI state
machine (status ball, fireplace, lightsaber, rainbow, a "Fireball" game,
settings, sleep, USB BOOTSEL). It contains no power-conversion code: no
references to PWM, dead-time, gate drivers, the LM5122 boost converter, the
H-bridge, the ADS7128 monitor, the INA240 current sense, the TMP235 temperature
sensors, or I²C. `V1.9.ino` is the first main-board firmware and the first to
touch the power stage; it has never run on hardware, and the power stage has
never been energized.

Target: RP2354A (Raspberry Pi), built with the arduino-pico core, using
`FastLED` for LED output and `pico/bootrom.h` for `reset_usb_boot()`. This is
**not** an ESP32 target — do not reach for ESP32-specific APIs such as
`ledcSetup()`. `analogWrite()` is not an appropriate PWM choice on this core
either; RP2354A hardware PWM uses the SDK's `pwm_*` API.

### Versioning convention

PIE has two heads, and both are **edited in place**: `V1.8.ino` for the
Lamoka1-DEV-Brain and `V1.9.ino` for the Lamoka1/2. Git history is the record of
what changed; do not create a new `V1.x` file for every edit. `V1.9.ino` is the
first revision that runs on the main Lamoka1/2 board (five LEDs, TMP235 on
GPIO 26/27, RP2350 TRNG entropy, plus the H-bridge power stage). It does not run
on the DEV-Brain unmodified, and V1.8 does not run on the Lamoka1/2 unmodified.

- Make changes directly in the head that owns the behaviour: V1.8 for the
  DEV-Brain interface, V1.9 for the Lamoka1/2.
- Keep each commit to one coherent change. The commit message and diff are the
  revision record.
- Do not revive an old `V1.0`–`V1.7` sketch from history into the working tree;
  read it in git if you need to compare behaviour.
- A change that alters behaviour must also update the matching
  `PIE-v1.x-information.md` in the same change, and `SOFTWARE/README.md` if
  board compatibility changes.
- If a future board needs a genuinely divergent firmware, start a new head
  rather than forking the two existing ones into a third copy.

### Firmware ↔ documentation contract

`SOFTWARE/Portal Interface Engine/PIE-v1.8-information.md` is a behavioural
description of `V1.8.ino` written for readers of the firmware. It documents the
button gestures, the UI states, the brightness and speed tables, the entropy
service, timings, and the main loop. `PIE-v1.9-information.md` does the same for
`V1.9.ino` (main-board pin map, five-LED interface, arming, PWM dead-time,
soft start, regulation, and protection).

If you change firmware behaviour, update the matching document in the same
change. A behaviour change with a stale description is a defect.

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

**Transformer.** The fitted part is a `VPT24-6670`, rated **160 VA**, mounted
off-board and wired to the board at J1. `HARDWARE/Transformer-wiring.md` records
the lead-to-terminal map. Read it before wiring anything: J1's eight poles are
four pairs, one pair per parallel-connected winding, and the lead colours must
**not** be matched to the net names — `/BLUE`, `/BROWN`, `/BLACK` and `/RED` are
lead colours, and this transformer's BLUE/BROWN leads are its 115 V winding,
while the board's bridge sits on the `/BLUE`/`/BROWN` nets. The `-6670` has the
same eight-lead colour map as the `-4170` it replaces, so the J1 harness map
does not change; the harness current capability does. Confirm the map against
the physical unit before the power stage is energized.

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
Everything else on the board is 21.75 V or below and stays on the 0.18 mm
netclass. The rule is what pulls the GND pour back from the secondary:
**refill the zones after any copper edit**, or the plotted pour will not obey
it.

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
  0.8 V. The bus calculation is correct as designed: on the Rev C board
  `1.2 × (1 + 64.9/3.79) = 21.749 V`. (The TPS62160 buck *does* use 0.8 V — the
  two differ, and that is not an error.)

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

#### Third-party review — Tomachie, 2026-10-19

The Lamoka1/2 schematic (4 sheets, 117 components, 495 pins, 84 nets) was
submitted to Tomachie and came back **93/100** on the free preliminary pass
(job `0a968485-5479-41e2-b24f-00a293061b1e`, report
<https://tomachie.com/r/0a968485-5479-41e2-b24f-00a293061b1e/Lamoka1-half_report.html>,
copy in `HARDWARE/Kicadfiles/Lamoka1-half/Reviews/`). The AI-enriched pass — the
datasheet-level functional and power analysis, the AI findings list, the
EMC/IEC checks — is a **paid** Tomachie add-on (13 coins ≈ US$13 via Stripe) and
was not bought; one-page designs get it free, this one does not. So the report
is the preliminary one, and it has no AI findings in it.

What it confirms, and what it adds:

- It independently confirms the **USB ESD gap**: `CC1`, `CC2` and the USB-C
  shell ground are listed as connector-facing nets with no ESD/TVS protection.
  That is the same item already listed below.
- **11 unconnected pins**, all benign: `IC1.4` and `U5`/`U6` NC pins, the USB-C
  shield pad, and `R9.1`. Nothing to change.
- **"4 power symbols connect nothing"** is a false positive: they are the four
  `Flamingo&Cactus-logo:LOGO` graphic placements, not rails.
- The **footprint-naming** deduction is the same IPC-7351B item as the DEV-Brain
  report had; it needs new part numbers, not a schematic edit.
- It inserted **18 test points** into a modified design. That modified schematic
  can be resubmitted for a better fault-coverage score, and the score they
  quote is a net-coverage figure, not a correctness judgement.

#### Findings from the 2026-10-18 reviews, and what Rev C did about them

Two electrical reviews read the netlist, `COMPONENTS.md`, `Operation.md` and the
LM5122, UCC27735, INA240, ADS7128, TMP235, AMS1117, TPS62160 and Triad
datasheets. Their findings are recorded here so the next pass re-derives and
decides rather than rediscovers. Several were **acted on in Rev C**; the
derivations are kept in `HARDWARE/revB-changes.md`, and the values below are
what is fitted. The usual rule still applies: re-derive from the source before
acting.

**Applied in Rev C.** These are in the schematic and the board now:

- **Transformer hook-up (the serious one) — harness, unchanged by the
  revision.** The board's bridge sits on the `/BLUE`, `/BROWN` nets and the
  outlet on `/BLACK`, `/RED`, but the transformer's BLUE/BROWN leads are its
  115 V winding and its 12 V winding is BLACK/YELLOW/RED/ORANGE. Fitted
  colour-for-colour, the bridge drives the 115 V winding and the outlet gets
  about 1.9 V instead of ~120 V. The eight-pole connector is already wired for
  the parallel connection of both windings, so the fix is a harness spec, not
  copper: `HARDWARE/Transformer-wiring.md`. The part is now the
  `VPT24-6670`; the eight-lead colour map is identical, so the harness map
  does not change.
- **`Rsense1` stays 2 mΩ — the 5 mΩ finding was wrong.** The earlier finding
  proposed 5 mΩ (the LM5122 procedure wants 4.1–5.6 mΩ) on the grounds that a
  2 mΩ shunt lets the inductor saturate before the current limit trips. That
  is wrong for a 100 W single-phase load: the 120 Hz bus-current pulsation
  gives an inductor crest of about 27 A, so a 5 mΩ shunt would clip the output
  to roughly 57 W. The shunt is 2 mΩ (`IPEAK(CL) = 75 mV / RS`, datasheet
  Eq. 13 — the ×10 gain belongs to the control-loop sense path, not to the
  current-limit comparator), and it moves from a 1206 to a **2512 2 W** part
  (`CSNL2512FT2L00`, Stackpole) purely for thermal headroom: the 1206 ran at
  68 % of its derated rating at a 120 °C ambient, the 2512 runs at 38 %. The
  part is a two-terminal device, so **no Kelvin sensing** is implemented.
- **`C13` was a 10 µF 0402 ceramic on the AMS1117-3.3 output; it is now a
  22 µF tantalum A case** (`T491A226K010AT`). The AMS1117 datasheet asks for
  22 µF and names tantalum: the ESR is part of the part's frequency
  compensation, and near-zero-ESR MLCC is the risky condition. This is a
  stability fix, not a nicety.
- **The gate drivers now have a local supply bypass.** `U5`/`U6` pin 7 (VDD) and
  pin 4 (EN) sit on `CSP`, the 11–14 V battery rail — **not** the 21.75 V bus
  — and the bootstrap diodes `D11`/`D12` are fed from `CSP` as well (checked
  against the netlist: `CSP` carries `U5`/`U6` pins 4 and 7 and the `D11`/`D12`
  anodes). The nearest bulk capacitor was 49–84 mm away; the datasheet asks
  for about 10 µF at the pin (`CVDD ≥ 10 × CBOOT` with the 1 µF bootstrap
  capacitors). Each driver now has a 10 µF 0805 (`C34`, `C36`) and a 100 nF
  0603 (`C35`, `C37`) at its VDD pin.
- **The outlet's earth is no longer the board ground.** `J5` pins 3/4 — the
  earth pole of the outlet — are on their own `/PE` net, not on the board's
  `GND`. The old tie made every GND point on the board an earth conductor: the
  mains outlet's protective earth and the inverter's low-voltage ground were
  the same node, and the 115 V secondary had no earth of its own. The cut is
  in both the schematic and the board; on the board, pads 3 and 4 are joined
  by a 2.40 mm track matching the `/BLACK` and `/RED` pole links, and the GND
  pour was refilled and pulled back from them. The harness side of it is not
  closed — see the open findings below.

**Still open.** These are harness, mechanical or design-intent items that Rev C
does not close:

- **No fuse anywhere.** Neither the 11–14 V input nor the high-voltage output is
  fused; the only series element on `+BATT` is a 2 mΩ measurement shunt. The
  input fuse and the output fuse are harness items, off-board
  (`HARDWARE/revB-changes.md`); nothing is fused on the board itself.
- **No output filter.** The bridge drives the transformer primary directly: the
  `/BLUE` and `/BROWN` midpoints run from `Q3`/`Q4`/`Q5`/`Q6` to `J1` with no
  series inductance, no shunt capacitance and no snubber, so the 12 V winding
  sees the raw 20 kHz square wave and the harmonics go straight to the
  outlet. The off-board output filter (series 38 mH, shunt 470 nF, damper,
  bleeder) is specified but not built.
- **The outlet's earth bond is a harness item.** The board's `J5` pins 3/4 are
  on their own `/PE` net, so the board no longer earths the outlet —
  `HARDWARE/Transformer-wiring.md` records the same map. The harness has to
  bond that net to a chassis PE stud for Class I (metal enclosure) or leave it
  unbonded and mark the unit double-insulated for Class II (plastic
  enclosure). A three-prong outlet must never ship with its earth floating or
  tied to the board's logic ground.
- **The TMP235 relocation** (`U9` closer to `Q4`/`Q6`, thermal vias under
  `Q1`/`Q2`) is still open; the sensors report board temperature, not FET
  junction temperature.
- **A direct isolated output sense (AMC1311B) is designed but not in this
  revision.** The outlet is regulated from the droop-compensated estimate,
  which needs a per-unit calibration at bring-up.

**Firmware status (V1.9, Rev C).** The first main-board firmware exists
(`SOFTWARE/Portal Interface Engine/V1.9.ino`): five-LED UI ported from V1.8, RP2350
TRNG entropy, TMP235 reads on GPIO 26/27, explicit arming, PWM dead-time scheme
verified by host-side simulation in `tests/test_v19_pwm.py` (500 ns on both
edges, both legs, full modulation range), soft-start ramp, closed-loop
regulation, and supervisory protection. The power stage is still **never
energised**; bench verification (scope dead-time check, ADC/current/
temperature validation, staged energisation) is outstanding. The audit's
minimum change set — `C13`, the `U5`/`U6` bypass and the `Rsense1` value —
is applied in Rev C; the fusing is not.

The 2026-10-19 pass applied these firmware fixes, and Rev C retargeted the
regulation. `V1.9` has still never run on hardware:

- The output target is **120.0 V RMS at the outlet**, on a **21.75 V bus**. The
  firmware regulates the **estimated** outlet, not the drive-side value:
  `V_out_est = A_DROOP × V_drive_rms − B_DROOP × I_sec_est`, with
  `A_DROOP = 8.8735` (the transformer's true no-load ratio, 115 / 12.96 — the
  nameplate 12.0 V is a *full-load* rating, so the no-load secondary is
  12.96 V) and `B_DROOP = 5.666 Ω`. The earlier `115/12 = 9.583` was the
  full-load ratio and made the firmware believe the outlet was 120 V when it
  was really about 111.5 V. Both constants are fitted per unit at bring-up:
  `A_DROOP` from the no-load ratio measured with a DMM, `B_DROOP` from two
  load points.
- A new **output out of reach** fault fires when the modulator is pinned at
  the PWM dead-time clamp (`m = 0.959733`) while the estimated outlet is still
  more than 0.5 V short of the target.
- The bridge-output-missing check is proportional to the commanded modulation
  (60 % of expected, 5 consecutive ticks) instead of a fixed fraction of the
  target.
- `G_VERY_LONG` disarms the bridge before its 120 ms blocking delay.
- The I²C `Wire` timeout is capped at 2 ms, so a stuck bus cannot freeze the
  bridge long enough to saturate the transformer.
- ADS7128 failures clear `adsPresent`/`adsSwept`/`adsPhase`, and
  `powerClearFault` forces a fresh probe and a full five-channel sweep.
- `G_DOUBLE` and the hold gesture clear a latched fault in the Inverter
  portal.
- The hardware watchdog is 500 ms and is fed during the boot sweep as well as
  from the main loop, so the boot sequence can no longer trip it.
- The DC-offset trip is 1 V over 3 ticks, replacing the 2 V threshold. The
  firmware's "~90 ms to saturate" comment predates the Rev C flux
  re-derivation, which puts the knee at roughly 11–13 ms at the operating
  point; the threshold is unchanged in Rev C and re-deriving it is an open
  item.
- The 60 Hz sine is written once per 20 kHz carrier from the slice-0 wrap ISR
  out of a 64-point table with interpolation, and the amplitude slew is limited
  to 0.010 per tick.
- The `AIN1` bus windows are **±0.90 V at start and ±1.5 V while running**,
  against the 25.74 V full scale of the new 68 kΩ divider.

The dead-time and PWM behaviour is checked by host-side simulation in
`tests/test_v19_pwm.py`; `tests/test_v19_model.py` is the new model-level
test. The older `tests/test_repo.py` only greps the sketch's source text — it
is a consistency check, not a behaviour check.

Still open on the board side: fusing, the output filter and the TMP235
relocation (see `HARDWARE/revB-changes.md`).

The V1.9 ADS7128 driver was corrected on 2026-10-18 to use the opcode framing
the part requires (write 0x08 / read 0x10 / set-bits 0x18 / block-read 0x30):
`STATS_EN` is set and verified before `RECENT_CHx` is trusted, `OSR_DONE` is
cleared write-1-to-clear after each conversion, and the result is read as a block
so both bytes belong to one conversion. The original transactions would have
NACKed or returned garbage. The corrected driver is still **unverified against
silicon** — a register-level bench check is the first monitor item at bring-up.

- **Dead time is entirely firmware's job.** The UCC27735 has interlock but no
  built-in dead time (datasheet §6.3.1); no hardware prevents overlap. V1.9
  writes the sine once per 20 kHz carrier from the slice-0 wrap ISR, so
  main-loop jitter cannot make the two phase-offset slices latch different duty
  values and breach the 500 ns gap — the interlock remains the last line of
  defence, not a substitute for the firmware dead time.
- **The bridge must stay gated off until the boost has finished start-up.**
  `Css1` (33 nF) is far below LM5122 Eq. 12 for a pre-loaded 587 µF bus; the
  design relies on the load being absent during soft start.
- **No gate-source bleed resistors on `Q1`–`Q6`.** The drivers hold the gates low
  while powered; a dead driver leaves them floating.
- **The AC output is not measured directly.** The monitor chain is the battery
  rail (`AIN0`), the 21.75 V bus (`AIN1`), the two bridge midpoints
  (`AIN2`/`AIN3`), load current (`AIN4`, INA240) and two TMP235s. The firmware
  estimates the outlet from the drive winding and the droop model, not from a
  measurement of it. V1.9's "bridge output missing" check compares the midpoint
  differential against the commanded modulation, so it watches the **bridge**,
  not the outlet: an open transformer secondary, a missing output filter or a
  disconnected load is invisible to it.
- **No USB ESD protection** on `USB-5v`, D+/D− or CC.
- **`AIN1` headroom — resolved.** The old divider (`R27` 43.2 k / `R28` 10 k)
  put 3.195 V on a 3.3 V input and clipped at 17.56 V, and the old midpoint
  dividers (`R29`/`R31` 45.3 k) clipped at 18.25 V — both below the Rev C bus,
  which is why they changed. `R27`, `R29` and `R31` are all **68 kΩ** now
  (`RC0603FR-0768KL`), giving a 25.74 V full scale, and the firmware's
  `DIV_AIN1`/`DIV_AIN2`/`DIV_AIN3` are `10/(68 + 10)`. The bus sits at 2.79 V,
  well below the 3.3 V clip.
- **The isolation barrier has no slack.** The closest approach is 1.5005 mm
  against the 1.5 mm rule (IPC-2221B asks 1.25 mm at 151–300 V); any future
  copper edit near `/BLACK`/`/RED` needs the barrier scan re-run. That figure
  was re-measured on the Rev C layout: nothing closer than 2.1 mm to any
  track, pad or via, and 1.5005 mm to the pulled-back GND zone fill, which is
  where the rule bites.

**Boost-converter observations.**

- **Plug-in inrush.** With a battery connected, the 587 µF bus charges through
  `Rsense1` → `L2` → `Q1`'s body diode with only milliohms of limiting, because
  the converter is unpowered and `Css1` cannot soft-start it. There is no fuse,
  NTC, precharge or hot-swap on `+BATT`.
- **No off switch.** `U4` has no enable pin and the UVLO divider is passive,
  although datasheet §7.3.1 offers the UVLO pin as a remote shutdown; the rail
  sits at ~VIN whenever a battery is attached, with no designed bleeder.
- **`Q1` voltage margin — resolved.** `Q1` was a 25 V part on the switching
  node at ~495 kHz with no snubber; on the Rev C bus that left only about 3 V
  of headroom for ringing. `Q1` is now `CSD17304Q3` (30 V), the same part as
  `Q2` and in the same DQG0008A footprint, which leaves about 8 V at the
  nominal bus and 7.6 V at the top of the tolerance band. No 40 V part exists
  in that footprint. The LM5122 drives both gates directly, as its typical
  application does.
- **Ripple.** 1.8 µH at ~495 kHz gives about 4.4 A pk-pk on a 5 A average
  (≈88 %), so the converter enters DCM below roughly 1.4 A at 14 V input.
- **Derived-but-undocumented values.** The Rev C compensation is now derived
  and written down: `Rslope1` 49.9 kΩ (the datasheet wants a slope
  compensation factor K ≥ 1; the old 105 kΩ gave 0.525), `Rcomp1` 80.6 kΩ,
  `Ccomp1` 15 nF, `Ccomp2` 44.8 pF, for a cross-over of about 9.3–11.8 kHz
  with roughly 80° of phase margin. `Cres1` is datasheet-consistent. `Css1`
  is the exception above.
- **Grounding.** `AGND`, `PGND` and the low-side returns are one `GND` net in the
  schematic; the star point is not expressed anywhere (`NT1` is a board net tie).
- **`MODE` strap.** `MODE` is tied to GND: diode emulation with the skip-cycle
  comparator disabled, i.e. pulse skipping at light load. Valid, but the choice
  is recorded nowhere.
- **Net names.** The input rail is `/Inversion-Section/CSP` and the 21.75 V bus
  is `/Inversion-Section/G4D` — named after unrelated pins, which makes the boost
  and the `.kicad_dru` rules that key on net names harder to audit.
- **Polarisation.** The electrolytic and polymer output capacitors are drawn with
  `Device:C`, whose description reads "Unpolarized capacitor".
- **Thermal.** The FETs' `DQG0008A` footprints carry no thermal vias by design;
  the resulting junction-temperature budget is not written down.
- The remaining DRC leftovers (four dangling GND vias, one isolated `1V1`
  sliver, two `J3` silkscreen clips) are the design intent documented above.

The stale board `Value` fields the same reviews flagged (`D6`, `D11`, `D12`,
`CR1`, `CR2`) were synced to the schematic on 2026-10-18 and no longer apply.

#### Rev B and Rev C change sets (what was found, and what was applied)

`HARDWARE/revB-changes.md` holds the written change set: what the 2026-10-18
reviews found, the derivations behind each item, and the Rev C section. It now
reads as a record rather than a forward plan.

Rev B's items split cleanly. The **board** items are applied in Rev C: the
`AIN1`/`AIN2`/`AIN3` divider change (`R27`, `R29`, `R31` 43.2 k / 45.3 k → 68 k),
the local `U5`/`U6` VDD bypass, the `C13` change to a 22 µF part with the ESR
the AMS1117 needs, and the `Q1` voltage rating. The **`Rsense1` value does not
change** — the 5 mΩ proposal was wrong and is withdrawn (see above). The
**harness** items are not applied and are still open: the off-board output
filter, the input and output fuses, the reverse-polarity protection, the `J5`
earth bond, and the TMP235 placement.

#### Rev C (applied)

The revision moves the board from a 17 V boost bus to a **21.75 V bus** and
from a 105 V inferred outlet target to a **120.0 V RMS outlet**, with a
160 VA transformer.

| Item | Rev B | Rev C | Why |
| --- | --- | --- | --- |
| `Rfbt1` | 49.9 kΩ | **64.9 kΩ** (`CRCW040264K9FKED`), `Rfbb1` 3.79 kΩ unchanged | The LM5122's 1.2 V reference sets the bus: `1.2 × (1 + 64.9/3.79) = 21.749 V`; band 21.13–22.39 V with 1 % resistors and a 1 % reference |
| `U7` TPS62160 | on `CSP` (11–14 V) | **no change** | U7 pins 2/3 are on `CSP`, not the bus, so the bus raise does not reach the 3.3 V buck in the present architecture. No new buck is needed |
| `Q1` | 25 V `CSD16323Q3` | **30 V `CSD17304Q3`** | Same DQG0008A footprint as `Q2`; about 8 V of headroom on the switching node. No 40 V part exists in that footprint |
| Transformer | `VPT24-4170`, 100 VA, 9 % | **`VPT24-6670`, 160 VA, 8 %**, 12.0 V @ 13.34 A | 100 W at 89 % efficiency is about 111 VA through the transformer, which is 111 % of a 100 VA rating. The 160 VA part runs at 69 %, and 100 W is 60 % of it instead of over 100 %. The eight-lead colour map is identical, so the J1 harness map does not change |
| `R27`/`R29`/`R31` | 68 kΩ | **68 kΩ** (as Rev B) | 25.74 V full scale covers the 21.75 V bus; the old 45.3 kΩ clipped at 18.25 V and read 74 % of the truth |
| Bus windows | ±0.75 V / ±1.5 V (as specified) | **±0.90 V / ±1.5 V** (as flashed) | The start window was widened from the specified ±0.75 V because the tolerance band is ±0.63 V, which left only 0.12 V of slack |
| Compensation | 105 k / 28.7 k / 3.9 nF / 130 pF | **49.9 k / 80.6 k / 15 nF / 44.8 pF** | Re-derived for the new operating point; cross-over about 9.3–11.8 kHz with roughly 80° of phase margin |
| Bus capacitors | 25 V 180 µF | **35 V 180 µF** (`35SVPK180M`), `Coutx1` 22 µF 35 V 1206 | The 25 V parts ran at 89.5 % of their rating on the new bus |
| `Rsense1` | 2 mΩ 1206 | **2 mΩ 2512 2 W** (`CSNL2512FT2L00`) | The value does not change; the package does, for thermal headroom. No Kelvin sensing |
| `C13` | 22 µF tantalum | **as Rev B** | The AMS1117 needs 22 µF with ESR |
| `U5`/`U6` bypass | 10 µF + 100 nF | **as Rev B** | `CVDD ≥ 10 × CBOOT` with the 1 µF bootstrap capacitors |
| `J5` earth | cut the 3/4-to-GND tie | **cut, applied** — `J5` pins 3/4 are on their own `/PE` net | Board GND is not protective earth; the harness provides the bond |

**The corrected transformer ratio.** The outlet is regulated from an estimate,
and the estimate depends on the transformer's *no-load* ratio. The nameplate
12.0 V is a **full-load** secondary rating, so the open-circuit secondary is
12.0 × 1.08 = 12.96 V and the true ratio is `115 / 12.96 = 8.8735`. Using
`115 / 12 = 9.583` — the full-load ratio — overstates the outlet by 8 %, and
the firmware would believe the outlet was 120 V when it was really about
111.5 V. The firmware now uses `A_DROOP = 8.8735` and `B_DROOP = 5.666 Ω`, and
both are fitted per unit at bring-up.

**What is still open after Rev C.** The off-board output filter, the outlet
fuse and MOV, and the input harness fuse, reverse-polarity protection and TVS
are harness items, not board copper. A direct isolated output sense (AMC1311B)
is designed but not in this revision, so the outlet is regulated from the
droop-compensated estimate, which needs a per-unit calibration. The `J5` earth
cut is on the board; the harness bond to a chassis PE stud is not. The
transformer's actual regulation is 8 % **TYP with no published limit**, and
the modulation headroom at the worst bus corner is about 2 %, so the bring-up
must measure the real transformer. The board has still never been energised.

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
  constants, one file per firmware head (V1.8 or V1.9), edited in place.
