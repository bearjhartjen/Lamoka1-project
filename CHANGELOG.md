# Changelog

All notable changes to the Lamoka1 project are documented in this file. The
format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

The project versions its artifacts separately: hardware revisions are released
as GitHub releases (for example `lamoka1_DEV_Brain_v1`), and the Portal Interface
Engine firmware follows its own revision sequence (`V1.0`–`V1.8`).

## [Unreleased]

### Added

- `HARDWARE/Mechanical/Lamoka1.step` — 3D STEP model of the Lamoka1/2 assembly
  (board outline, holes, and component bodies) for enclosure and fit checks.
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

### Changed

- Lamoka1/2 main board: routed the control, boost, and inversion sections and
  updated component placement and footprints to match vendor data.

## [lamoka1_DEV_Brain_v1] — 2026-10-02

### Added

- First released revision of the Lamoka1-DEV-Brain, the control board used to
  develop the Portal Interface Engine (PIE) firmware on real hardware.
- Bench-tested and 72-hour burn-in passed. The exact tested configuration is
  available through the PCBWay shared project, and the board pairs with PIE
  V1.8 for an out-of-box testable experience.

[Unreleased]: https://github.com/bearjhartjen/Lamoka1-project/compare/lamoka1_DEV_Brain_v1...HEAD
[lamoka1_DEV_Brain_v1]: https://github.com/bearjhartjen/Lamoka1-project/releases/tag/lamoka1_DEV_Brain_v1
