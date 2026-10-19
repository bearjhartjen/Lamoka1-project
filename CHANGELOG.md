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
- `tools/make_fab_bom.py` — builds that BOM (70 lines, 112 placements) from a
  KiCad netlist, applying a hand-maintained manufacturer and description map.
- `HARDWARE/Transformer-wiring.md` — the VPT24-4170 lead-to-terminal spec for
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
  - The output target is 105 V RMS, not 115 V: 115 V needs m ≈ 1.10 at full load
    on the 17 V bus (`PWM_M_MAX` is 0.9597). The AIN1 bus windows are ±0.4 V,
    below the 17.56 V clip. Current build: 84,372 B program / 16,204 B globals.
- `tests/test_v19_pwm.py` and `tests/test_v19_model.py` — host-side models of
  the three-slice PWM dead time and the protection arithmetic, so these
  behaviours are no longer checked only by source-text matching.
- `HARDWARE/revB-changes.md` — the staged Rev B change set (off-board output
  filter, input and output fuses, reverse-polarity protection, AIN1 divider,
  gate-driver bypass, C13, Rsense1, J5 earth, TMP235 placement) plus the
  separate Rev C 115 V option. Nothing in it is applied.
- `AGENTS.md`, `PIE-v1.9-information.md`, `README.md`, and `SOFTWARE/README.md`
  now state the 105 V inferred target and the roughly 96–105 V outlet range,
  correct the gate-driver VDD rail to CSP, and record the open hardware
  findings.

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
