# Changelog

All notable changes to the Lamoka1 project are documented in this file. The
format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

The project versions its artifacts separately: hardware revisions are released
as GitHub releases (for example `lamoka1_DEV_Brain_v1`), and the Portal Interface
Engine firmware follows its own revision sequence (`V1.0`–`V1.9`).

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
- Named the main board **Lamoka1/2** in prose and `Lamoka1-half` in files and
  paths, everywhere: `HARDWARE/Kicadfiles/Lamoka1-half/` with `Lamoka1-half.*`
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
  Lamoka1/2 views now show the October 2026 revision (the isolation fix, the
  moved references, and the "DANGER! HIGH VOLTAGE" legend between `J1`'s pad
  rows). Each render is trimmed of its transparent border, so the pictures hug
  the boards instead of carrying empty margins.

### Removed

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
