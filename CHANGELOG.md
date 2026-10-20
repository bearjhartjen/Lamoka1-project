# Changelog

All notable changes to the Lamoka1 project are documented in this file. The
format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

The project versions its artifacts separately: hardware revisions are released
as GitHub releases (for example `lamoka1_DEV_Brain_v1`), and the Portal Interface
Engine firmware is kept as two in-place heads (`V1.8` and `V1.9`); earlier
revisions remain in git history.

## [Unreleased]

### Added

- `HARDWARE/Mechanical/` — STEP models of both boards for enclosure and fit
  checks: `Lamoka1-half.step` for the Lamoka1/2 and `Lamoka1-DEV-Brain.step`
  for the control board, each with board outline, holes, and component bodies.
- `HARDWARE/Mechanical/README.md` — how the mechanical models are generated.
- `Images/lamoka1-dev-brain-render-iso.png` and `-top.png` — `kicad-cli pcb
  render` views of the DEV-Brain, matching the Lamoka1/2 renders.
- `docs/` documentation index, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, and
  `SECURITY.md`, plus GitHub issue and pull-request templates.
- `LICENSES/` directory with the SPDX license texts used by the project.
- `Makefile` with the common tasks (`make firmware`, `make test`, `make check`,
  `make static`, `make step`), and `tests/` with the repository checks (links,
  BOM, ERC/DRC reports, firmware pin contract, credentials scan).
- GitHub Actions workflow (`.github/workflows/checks.yml`) that runs the
  repository checks, cppcheck static analysis, and a firmware build on every
  push and pull request.
- README: hardware status table, quick-start section, build-and-fabrication
  guide (ordering, assembly, staged bring-up), firmware flashing instructions,
  and regenerated board renders.
- Pinned Python development environment for the repository checks:
  `pyproject.toml`, `uv.lock`, and `.python-version`, with `make lint`
  (ruff), a lint CI job, and `.editorconfig`.
- `.env.example` for optional local Makefile overrides, and Dependabot
  configuration for the GitHub Actions and uv ecosystems.
- `HARDWARE/Lamoka1-half fab files/` — the main board's fabrication and assembly
  submission, mirroring the DEV-Brain package: nine Gerber layers, the drill
  file, drill map and report, the PCBWay-format assembly BOM, pick-and-place for
  both sides, and a README covering stackup, assembly, ordering notes and the
  accepted DRC warnings.
- `tools/make_fab_bom.py` — builds that BOM (70 lines, 116 placements) from a
  KiCad netlist, applying a hand-maintained manufacturer and description map.
- `HARDWARE/Transformer-wiring.md` — the VPT24-6670 lead-to-terminal spec for
  `J1`, with the mis-wire hazards. The `J1` net names are lead colours and the
  H-bridge sits on the `/BLUE`/`/BROWN` nets, so the transformer leads must not
  be fitted colour-for-colour; fitted correctly the bridge drives the 12 V
  winding and the outlet takes the 115 V winding.
- `tools/make_images.sh` and `make images` — regenerate every board picture in
  `Images/` from the KiCad layouts: the three ray-traced Lamoka1/2 views, the
  routing plot of the F.Cu and B.Cu layers, and the two DEV-Brain views (which
  additionally need that board's local 3D models). The pictures could previously
  be reproduced only by hand.
- `HARDWARE/Lamoka1-half fab files/Lamoka1-half interactive BOM.html` — a
  self-contained InteractiveHtmlBom view of the Lamoka1/2 assembly, with the
  BOM, placement outlines, nets and tracks embedded.
- `U4` now carries the `Texas_PWP0020A` 3D model, so the LM5122 appears in the
  board renders and in `HARDWARE/Mechanical/Lamoka1-half.step`.
- `SOFTWARE/Portal Interface Engine/V1.9.ino` and
  `PIE-v1.9-information.md` — the first main-board (Lamoka1/2) firmware: the
  five-LED interface ported from V1.8, RP2350 TRNG entropy, TMP235 reads on
  GPIO 26/27, and the power stage — explicit arming, PWM dead-time, soft start,
  closed-loop regulation, and supervisory protection. The ADS7128 monitor is
  driven with opcode-framed transactions, which the part requires (a bare
  register address is not a valid access). Compiles clean (84,964 B program,
  15,884 B globals); it has never run on hardware and the bench checks
  (dead-time scope verification, ADC/current/temperature validation, staged
  energisation) are outstanding.

### Changed

- **Rev C — the main board now targets 120.0 V RMS at the outlet.** The power
  topology is 11–14 V battery → LM5122 boost → **21.75 V bus** → H-bridge →
  12 V winding of a Triad **VPT24-6670** → 115 V winding → 120.0 V outlet.
  The board and firmware changes are applied; the derivations and the still-open
  items are in [`HARDWARE/revB-changes.md`](HARDWARE/revB-changes.md).
  - **Boost bus 17 V → 21.75 V.** `Rfbt1` 49.9 kΩ → **64.9 kΩ**
    (`CRCW040264K9FKED`), `Rfbb1` unchanged at 3.79 kΩ:
    `1.2 × (1 + 64.9/3.79) = 21.749 V`, band 21.13–22.39 V with 1 % resistors
    and a 1 % reference.
  - **Output target 105 V → 120.0 V RMS at the outlet.** The firmware no longer
    regulates the inferred drive-side value; it regulates a droop-compensated
    **estimate** of the outlet,
    `V_out = A_DROOP × V_drive_rms − B_DROOP × I_sec_est`, with
    `A_DROOP = 8.8735` (the transformer's *true no-load* ratio, 115 / 12.96 —
    the nameplate 12.0 V is a full-load rating) and `B_DROOP = 5.666 Ω`. Both
    are fitted per unit at bring-up. A new **output out of reach** fault fires
    when the modulator is pinned at the PWM dead-time clamp while the estimate
    is still short of the target.
  - **Transformer `VPT24-4170` → `VPT24-6670`** (100 VA / 9 % → 160 VA /
    8 % typical, 12.0 V @ 13.34 A). The eight-lead colour map is identical, so
    the `J1` harness map does not change; the harness current capability rises
    and the unit is 1.6 kg instead of 1.0 kg.
  - **`Rsense1` stays 2 mΩ but moves 1206 → 2512** (`CSNL2512FT2L00`, 2 W,
    Stackpole — the BOM credited Bourns wrongly). The 1206 part ran at 68 % of
    its 120 °C derated rating; the 2512 runs at 38 %. The part is a two-terminal
    device, so **no Kelvin sensing** is implemented.
  - **`Q1` `CSD16323Q3` (25 V) → `CSD17304Q3` (30 V)**, the same part as `Q2`
    and the same DQG0008A footprint. On the 21.75 V bus the 25 V part left
    only about 3 V of headroom for switching-node ringing; the 30 V part
    leaves about 8 V. No 40 V part exists in that footprint.
  - **LM5122 compensation re-derived** for the new operating point:
    `Rslope1` 105 kΩ → **49.9 kΩ** (the fitted value gave a slope-compensation
    factor of 0.525 where the datasheet asks for K ≥ 1), `Rcomp1` 28.7 kΩ →
    **80.6 kΩ**, `Ccomp1` 3.9 nF → **15 nF**, `Ccomp2` 130 pF → **44.8 pF**.
    Cross-over moves to about 9.3–11.8 kHz with roughly 80° of phase margin.
  - **AIN dividers.** `R27` (AIN1), `R29` (AIN2) and `R31` (AIN3) are all
    **68 kΩ**. The old values clipped at the new bus — 45.3 kΩ gave an 18.25 V
    full scale, so the 21.75 V bus saturated the ADC and the firmware read
    74 % of the truth. 68 kΩ gives a 25.74 V full scale, and the firmware's
    `DIV_AIN1/2/3` are now `10/(68 + 10)`.
  - **Bus monitor windows** ±0.4 V → **±0.90 V start, ±1.5 V run**.
  - **Bus capacitors.** `Cout1`–`Cout3` 25 V / 180 µF → **35SVPK180M** (35 V,
    180 µF, same 8 × 11.9 mm land pattern); the old parts ran at 89.5 % of
    their rating. `Coutx1` 47 µF 1206 → **22 µF 1206 35 V**
    (`C3216X5R1V226M160AC`) — a 47 µF 1206 at 35 V does not exist.
  - **`C13`** 10 µF 0402 MLCC → **22 µF tantalum A case** (`T491A226K010AT`).
    The AMS1117 needs 22 µF *with* ESR; a near-zero-ESR X5R MLCC is the
    instability condition, so this is a stability fix, not a nicety.
  - **New gate-driver VDD bypass.** `C34`/`C36` (10 µF 0805) and `C35`/`C37`
    (100 nF 0603) sit at each of `U5` and `U6`'s VDD pin (pin 7). The
    UCC27735 datasheet wants `CVDD ≥ 10 × CBOOT` and the bootstrap capacitors
    are 1 µF; the nearest bulk capacitor used to be 49–84 mm away.
  - **`J5` outlet earth.** The board's logic ground is not protective earth,
    and the old tie made every GND point on the board an earth conductor.
    Rev C cuts that tie: the earth pole is now its own `/PE` net, to be bonded
    in the harness to a chassis PE stud for a metal enclosure (Class I), or
    left unbonded with the unit marked double-insulated (Class II) for a
    plastic one. The cut is in the schematic and on the board — `J5` pins 3/4
    are on `/PE`, not on `GND` — and the harness still has to provide the
    bond.
  - The **1.5 mm isolation rule** and its 1.5005 mm closest approach are
    unchanged and re-verified by DRC against the Rev C layout.
- The Lamoka1/2 board stackup now records the ordered finish: green solder mask
  with black legend on both sides. `tools/make_images.sh` renders both boards
  with `--use-board-stackup-colors`, so `Images/lamoka1-2-render-*.png` show the
  green board that PCBWay is building rather than KiCad's default mask colour.
  The Gerber job file was regenerated with the same colour metadata.
- Mains isolation: `Lamoka1-half.kicad_dru` now carries a 1.5 mm clearance rule
  around the 120 V transformer secondary (IPC-2221B table 6-1 column B2 asks
  1.25 mm). The BLACK run moved 0.6 mm clear of the control bus on both layers
  and the zones were refilled; the `Rvin1` and `R19` references moved off the
  pads they were clipping, the connector references (J1, J2, J4, J5, J6) are on
  silkscreen, and the "DANGER! HIGH VOLTAGE" legend sits between J1's pad rows.
- Corrected hardware details in the documentation: the fan wires to two solder
  pads rather than a Molex header, the 5 V OR-ing diodes are CR1/CR2 (D11/D12
  are the gate-driver bootstraps), the BOM export's real column names and its
  missing manufacturer column, the BOOTSEL route into the boot ROM, the
  transformer weight, and the now-clean ERC status.
- Untracked the project-local symbol libraries `Lamoka1-half_Parts.kicad_sym` and
  `Lamoka1-half_MOSFET.kicad_sym`, which the **Removed** entry below already
  described as no longer committed.
- Renamed `HARDWARE/Mechanical/Lamoka1.step` to `Lamoka1-half.step`: the folder
  now carries models for more than one board, so each export is named for the
  board it comes from. `make step` regenerates both board exports.
- Clarified that **Lamoka1/2** is the model designation, not a prose spelling
  of "main board"; `Lamoka1-half` is only its path-safe filesystem alias. The
  naming note in `AGENTS.md` and the fab-package README were rewritten to say
  so, and the main-board model name is used consistently in documentation.
- Named the main board model **Lamoka1/2**, with `Lamoka1-half` as the
  path-safe filesystem alias, everywhere: `HARDWARE/Kicadfiles/Lamoka1-half/` with `Lamoka1-half.*`
  project files, the project-local libraries and their `fp-lib-table` /
  `sym-lib-table` nickname, the backups directory, and the fab and mechanical
  exports. Repository name, branch and the DEV-Brain keep their names. Verified
  connectivity-neutral: the exported netlist is identical apart from its source
  path line, and the board's pads, vias, tracks and zones match; the renamed
  `.kicad_dru` isolation rule still loads and the zone fills reproduce.
- Synced five stale footprint `Value` fields on the board (`D6`, `D11`, `D12`,
  `CR1`, `CR2`) to the schematic, so the pick-and-place `Val` column agrees with
  the assembly BOM, which takes its values from the schematic.
- Documented the schematic-parity warnings rather than hiding them:
  `kicad-cli pcb drc --schematic-parity` reports 17 symbol-versus-footprint
  differences (U4's thermal-via pads, MPN fields that live on symbols,
  BOM-exclusion attributes that live on footprints), all deliberate; the
  fabricated DEV-Brain reports 94 of the same kind. A DRC run without
  `--schematic-parity` prints "0 Footprint errors" and hides them, which is now
  recorded in `AGENTS.md` and `CONTRIBUTING.md`.
- Regenerated `HARDWARE/Mechanical/Lamoka1-half.step` from the renamed board so
  the model's internal product name follows the board; the geometry is
  unchanged.
- `make test` now runs the check suite through pytest (the checks themselves
  are unchanged), and CI runs the checks and the new lint job through the
  pinned uv environment.
- CI runs the repository checks on Python 3.11 and 3.14, pins the Arduino
  toolchain it was verified against (arduino-cli 1.5.1, rp2040 core 6.1.1,
  FastLED 3.10.5), and limits the workflow to read-only repository
  permissions.
- Lamoka1/2 main board: routed the control, boost, and inversion sections and
  updated component placement and footprints to match vendor data.
- Regenerated the board pictures in `Images/` from the current layouts: the
  Lamoka1/2 views now use the green stackup and show the October 2026 revision (the isolation fix, the
  moved references, and the "DANGER! HIGH VOLTAGE" legend between `J1`'s pad
  rows). Each render is trimmed of its transparent border, so the pictures hug
  the boards instead of carrying empty margins.
- README: simplified for readability. The OpenSSF best-practices and RepoGrade
  badge links are removed, the page now leads with a plain-language overview and
  a project-status table, and the specifications, quick-start,
  build-and-fabrication, firmware, license, and AI-use sections are shorter. The
  section anchors other documents link to (`#specifications`, `#quick-start`,
  `#build-and-fabrication`, `#license`, `#usage-of-ai-in-this-repository`) are
  unchanged.

### Fixed

- PIE V1.9 firmware, before first power-on (the power stage has still never been
  energised):
  - The fixed-threshold output-missing check tripped during the soft-start ramp
    (at m = 0.05 the fundamental is 5.76 V); it now scales with the commanded
    modulation, runs only above 15 V expected, and uses a 5-tick debounce.
  - `G_VERY_LONG` now disarms the bridge before its 120 ms blocking delay.
  - The `Wire` timeout is capped at 2 ms so a stuck I²C bus cannot freeze the
    bridge; ADS7128 failures reset the probe state, and `powerClearFault` forces
    a fresh probe and a full sweep.
  - The Inverter portal's double tap and hold now clear a latched non-over
    fault; the parser treats the portal as a non-menu so the double tap is
    produced.
  - The hardware watchdog is 500 ms and is fed during the boot sweep as well as
    from the main loop.
  - The DC-offset trip is 1 V over 3 consecutive ticks.
  - The 60 Hz sine is written once per 20 kHz carrier from the slice-0 wrap ISR
    out of a 64-point interpolated table, and the amplitude slew is limited to
    0.010 per tick. The old main-loop update could make the phase-offset slices
    latch different duty values and breach the 500 ns dead time.
  - The output target is **120.0 V RMS at the outlet**, regulated from a
    droop-compensated estimate of the outlet rather than the inferred
    drive-winding value; the boost bus is 21.75 V nominal. The AIN1 bus
    windows are ±0.90 V at start and ±1.5 V while running. The old 105 V
    target was a firmware derating made on the 17 V bus, where 115 V needed
    m ≈ 1.10 at full load and `PWM_M_MAX` is 0.9597. Current build: 84,372 B
    program / 16,204 B globals.
- `tests/test_v19_pwm.py` and `tests/test_v19_model.py` — host-side models of
  the three-slice PWM dead time and the protection arithmetic, so these
  behaviours are no longer checked only by source-text matching.
- `HARDWARE/revB-changes.md` — the change set found by the 2026-10-18
  reviews, now written as a record of what was **found** and what was
  **applied**, plus the Rev C section. The off-board output filter, the input
  and output fuses, the reverse-polarity protection and the TMP235 placement
  are still open; they are harness or mechanical items, not board copper.
- `AGENTS.md`, `PIE-v1.9-information.md`, `README.md`, and `SOFTWARE/README.md`
  now state the 120.0 V outlet target on the 21.75 V bus, the droop-compensated
  estimate the firmware regulates, the corrected transformer ratio, and the
  open hardware findings.

### Corrected

- The **5 mΩ current-sense finding was wrong and is withdrawn.** The 2026-10-18
  reviews proposed moving `Rsense1` from 2 mΩ to 5 mΩ on the strength of the
  LM5122 procedure's 4.1–5.6 mΩ window. For a 100 W single-phase load the
  120 Hz bus-current pulsation gives an inductor crest of about 27 A, so a
  5 mΩ shunt would clip the output to roughly 57 W. `Rsense1` stays at 2 mΩ.
  It does change package, 1206 → 2512 (`CSNL2512FT2L00`, 2 W, Stackpole), for
  thermal reasons, and the part is a two-terminal device so no Kelvin sensing
  is implemented.
- The **`115/12` transformer ratio was wrong** for the outlet. The nameplate
  12.0 V is a *full-load* secondary rating, so the true no-load ratio is
  115 / (12.0 × 1.08) = 8.8735, not 9.5833. Using 9.5833 made the firmware
  believe the outlet was 120 V when it was really about 111.5 V.

### Removed

- PIE revisions `V1.0`–`V1.7` (`V1.ino` and `V1.1.ino`–`V1.7.ino`) are removed
  from the working tree. PIE is now two in-place heads, `V1.8.ino` for the
  Lamoka1-DEV-Brain and `V1.9.ino` for the Lamoka1/2, with git history as the
  revision record. `SOFTWARE/README.md`, `SECURITY.md`, `AGENTS.md`,
  `CONTRIBUTING.md`, the README, and the repository checks now describe the
  two-head layout.
- KiCad library files are no longer committed. The project-local libraries
  (`Lamoka1-half.pretty`, `Lamoka1-half_Parts.kicad_sym`, `Lamoka1-half_MOSFET.kicad_sym`)
  stay local and ignored, like the vendor libraries; the schematics and board
  carry embedded copies of everything the design needs (see `AGENTS.md`).

## [lamoka1_DEV_Brain_v1] — 2026-10-02

### Added

- First released revision of the Lamoka1-DEV-Brain, the control board used to
  develop the Portal Interface Engine (PIE) firmware on real hardware.
- Bench-tested and 72-hour burn-in passed. The exact tested configuration is
  available through the PCBWay shared project, and the board pairs with PIE
  V1.8 for an out-of-box testable experience.

[Unreleased]: https://github.com/bearjhartjen/Lamoka1-project/compare/lamoka1_DEV_Brain_v1...HEAD
[lamoka1_DEV_Brain_v1]: https://github.com/bearjhartjen/Lamoka1-project/releases/tag/lamoka1_DEV_Brain_v1
