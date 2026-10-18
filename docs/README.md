# Documentation index

This folder is the entry point to the Lamoka1 documentation. The authoritative
sources live next to the design files and firmware they describe; this page
links to them.

## Getting started

- [Quick start](../README.md#quick-start) — clone the repository, open the
  KiCad project, and build the current firmware.
- [Build and fabrication](../README.md#build-and-fabrication) — toolchain
  installation, ordering the PCB, assembling the board, staged bring-up, and
  flashing the firmware onto the Lamoka1-DEV-Brain.

## Using the project

- [Specifications](../README.md#specifications) — what the Lamoka1/2 does today.
- [Operation guide](../SOFTWARE/Operation.md) — the intended power-up and
  boot-sequence behavior.
- [PIE v1.8 information](../SOFTWARE/Portal%20Interface%20Engine/PIE-v1.8-information.md)
  — button gestures, UI states, brightness and speed tables, and timings of the
  current firmware.
- [Lamoka1-DEV-Brain guide](../Lamoka1-DEV-Brain/Readme.md) — the control
  board, its test results, and how to order it.

## Hardware design

- [Design notes](../HARDWARE/Design_Notes.md) — design intent and rationale.
- [Component reference](../HARDWARE/COMPONENTS.md) — plain-English component
  list (not the BOM; the schematic is authoritative).
- KiCad sources: [`HARDWARE/Kicadfiles/Lamoka1/`](../HARDWARE/Kicadfiles/Lamoka1)
  for the main board and [`Lamoka1-DEV-Brain/`](../Lamoka1-DEV-Brain) for the
  control board.
- 3D assembly model:
  [`HARDWARE/Mechanical/Lamoka1.step`](../HARDWARE/Mechanical/Lamoka1.step)

## Security and safety

- [Security policy](../SECURITY.md) — how to report vulnerabilities privately,
  and what is in scope.
- [Safety notes](../README.md#build-and-fabrication) — the Lamoka1/2 produces
  mains voltage. Never energize a board you built without completing the staged
  bring-up, and never leave a powered unit unattended.
- The firmware is developed against the Lamoka1-DEV-Brain; the power stage has
  no control firmware yet, so a freshly built Lamoka1/2 will not produce output
  in its current state.

## Contributing

- [Contributing guide](../CONTRIBUTING.md) — development setup, conventions,
  testing, and the pull-request checklist.
- [`Makefile`](../Makefile) — `make help` lists the common tasks: firmware
  build and upload, repository checks, ERC/DRC reports, static analysis, and
  STEP export. The same checks run in CI on every push.
- [Code of Conduct](../CODE_OF_CONDUCT.md)
- [Changelog](../CHANGELOG.md)
