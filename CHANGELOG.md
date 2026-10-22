# Changelog

What is in the project now, and what changed most recently. Git history has the
full record.

The project versions its artifacts separately: hardware is released as GitHub
releases (for example `lamoka1_DEV_Brain_v1`), and the Portal Interface Engine
firmware is kept as two in-place heads (`V1.8` and `V1.9`).

## Unreleased

### Added

- `HARDWARE/BRINGUP.md` — the plain-language bring-up guide for both boards:
  the DEV-Brain first, then the Lamoka1/2 in stages, with what to expect at
  every step.
- `tests/test_v19_pwm_waveform.py` — an independent host-side model of the
  RP2040/RP2350 PWM peripheral, built from the peripheral's own semantics
  rather than from the sketch's constants. It checks the dead time on both
  edges of both legs (statically and with the per-carrier re-latch), that the
  carrier-average differential is exactly `m·Vbus` — the model the droop
  estimator relies on — and that the drive is a 20 kHz PWM whose total RMS is
  about 1.16× its 60 Hz fundamental at the design point.
- `tests/test_v19_protection_latency.py` — the latency of every protection
  trip, measured from the code's own constants against the transformer's
  saturation knee, plus the ADS7128 register contract (opcodes, addresses and
  bit positions) against TI SBAS868A.
- `SOFTWARE/Portal Interface Engine/V1.9-audit-2026-10-20.md` — the audit
  record for the V1.9 firmware: findings, the derivation behind each, what was
  fixed and what still needs the bench.
- `HARDWARE/Mechanical/` — STEP models of both boards for enclosure and fit
  checks: `Lamoka1-half.step` for the Lamoka1/2 and `Lamoka1-DEV-Brain.step`
  for the control board, each with board outline, holes, and component bodies.
- `docs/` documentation index, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, and
  `SECURITY.md`, plus GitHub issue and pull-request templates.
- `Makefile` with the common tasks (`make firmware`, `make test`, `make check`,
  `make static`, `make step`, `make images`), and `tests/` with the repository
  checks (links, BOM, ERC/DRC reports, firmware pin contract, credentials
  scan).
- GitHub Actions workflow (`.github/workflows/checks.yml`) that runs the
  repository checks, cppcheck static analysis, and a firmware build on every
  push and pull request.
- `HARDWARE/Lamoka1-half fab files/` — the main board's fabrication and
  assembly submission, mirroring the DEV-Brain package: nine Gerber layers, the
  drill file, drill map and report, the PCBWay-format assembly BOM,
  pick-and-place for both sides, and a README covering stackup, assembly,
  ordering notes and the accepted DRC warnings.
- `tools/make_fab_bom.py` — builds that BOM (70 lines, 116 placements) from a
  KiCad netlist, applying a hand-maintained manufacturer and description map.
- `HARDWARE/Transformer-wiring.md` — the VPT24-6670 lead-to-terminal spec for
  `J1`, with the mis-wire hazards. The `J1` net names are lead colours and the
  transformer's leads do **not** go on colour-for-colour.
- `tools/make_images.sh` — regenerates the board renders in `Images/`.

### Changed

- **The documentation was rewritten to read as one straightforward path.**
  The README, the docs index, the firmware documents and the hardware notes
  now describe the design as it is, in plain language, with no revision
  archaeology and no "designed but not implemented" hedging.
- The outlet is now written as **120 V RMS** everywhere, not 120 V.
- The Lamoka1/2 is described as what it is: a prototype, the MVP, and the test
  unit for the inverter-only Lamoka1. The DEV-Brain is a prototype of the
  control section, and a product in its own right.
- **The droop constants are no longer fitted per unit.** `A_DROOP` and
  `B_DROOP` are the transformer's catalogue values, used as they come, and no
  unit has to be calibrated. A host-side test now proves the bound: the
  catalogue constants hold the real outlet within 3 % of 120.0 V RMS across
  the load range for a transformer regulating 6–10 %, which brackets the 8 %
  TYP catalogue value.
- **The "nothing senses the output" limitation is stated up front** in the
  README, the bring-up guide, the firmware header and the V1.9 document,
  rather than buried in a design note.
- The board renders are regenerated with `--quality high`, so the solder mask
  reads green instead of blowing out to white, and the camera framing fits the
  tall parts instead of cropping them.
- The Lamoka1/2 board stackup's silkscreen colour is now **white**, matching
  what PCBWay actually produces on a green-mask board. The committed Gerber
  job file was updated to match.

### Fixed

- The V1.9 measurement window is time-based (16.667 ms of `micros()`
  timestamps) rather than 128 samples. The old count-based window ran 21–26 ms
  at the loop's real 130–260 µs cadence, and the uncancelled 60 Hz fundamental
  false-tripped the DC-offset check during soft start. The firmware now reaches
  its 120 V regulation target instead of stopping at 12–13 V.
- The V1.9 build fails at compile time unless `F_CPU` is 150 MHz: the PWM
  constants produce 60 Hz only at that clock, and a board-menu frequency change
  would otherwise silently give 50 Hz or 80 Hz.
- The DC-offset trip now reacts inside the transformer's saturation window:
  the threshold is 0.5 V and the trip is evaluated on every RMS sample, not on
  the 100 Hz control tick. See the V1.9 audit record for the derivation.
- `OSR_DONE` is cleared with a plain register write rather than the set-bits
  opcode, and a missing `OSR_DONE` no longer faults the inverter.
- The bus run window tolerates three consecutive out-of-range ticks, so a load
  dump does not stop the inverter.

### Removed

- `HARDWARE/revB-changes.md` — the written change set for the old board
  revisions. The derivations that still matter live in `AGENTS.md`; the rest is
  in git history.

## lamoka1_DEV_Brain_v1 — 2026-10-02

The Lamoka1-DEV-Brain was manufactured and assembled by PCBWay, and the
firmware was bench-tested on it. See the
[DEV-Brain guide](Lamoka1-DEV-Brain/Readme.md).
