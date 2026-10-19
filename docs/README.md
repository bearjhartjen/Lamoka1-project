# Documentation index

This folder is the door into the Lamoka1 documentation. The authoritative
sources live next to the design files and firmware they describe — the KiCad
schematics, the firmware, and the reports are the specification — and this page
links to them. Nothing about how the Lamoka works is hidden; you just have to
know where to look.
## Getting started

- [Quick start](../README.md#quick-start) — clone the repository, open the
  KiCad project, and build the firmware.
- [Build and fabrication](../README.md#build-and-fabrication) — toolchain
  installation, ordering the PCB, assembling the board, staged bring-up, and
  flashing the firmware onto the Lamoka1-DEV-Brain.

## Using the project

- [Specifications](../README.md#specifications) — what the Lamoka1/2 does today.
- [Operation guide](../SOFTWARE/Operation.md) — the intended power-up and
  boot-sequence behavior.
- [PIE v1.8 information](../SOFTWARE/Portal%20Interface%20Engine/PIE-v1.8-information.md)
  — button gestures, UI states, brightness and speed tables, and timings of the
  DEV-Brain firmware.
- [PIE v1.9 information](../SOFTWARE/Portal%20Interface%20Engine/PIE-v1.9-information.md)
  — the main-board firmware: the five-LED interface plus arming, PWM dead-time,
  soft start, closed-loop regulation, and supervisory protection. Never run on
  hardware.
- [Lamoka1-DEV-Brain guide](../Lamoka1-DEV-Brain/Readme.md) — the control
  board, its test results, and how to order it.

## Hardware design

- [Design notes](../HARDWARE/Design_Notes.md) — design intent and rationale.
- [Component reference](../HARDWARE/COMPONENTS.md) — plain-English component
  list (not the BOM; the schematic is authoritative).
- KiCad sources: [`HARDWARE/Kicadfiles/Lamoka1-half/`](../HARDWARE/Kicadfiles/Lamoka1-half)
  for the main board and [`Lamoka1-DEV-Brain/`](../Lamoka1-DEV-Brain) for the
  control board.
- 3D models: [`HARDWARE/Mechanical/`](../HARDWARE/Mechanical) — STEP exports of
  both boards (main board and DEV-Brain) with component bodies.
- Board pictures: [`Images/`](../Images) — ray-traced views of both boards
  (the Lamoka1/2 in its ordered green mask) and the Lamoka1/2 routing plot;
  `make images` regenerates them from the layouts.
- Assembly: [`HARDWARE/Lamoka1-half fab files/`](../HARDWARE/Lamoka1-half%20fab%20files)
  — fabrication package, BOM, pick-and-place and the
  [interactive BOM](../HARDWARE/Lamoka1-half%20fab%20files/Lamoka1-half%20interactive%20BOM.html).

## Security and safety

- [Security policy](../SECURITY.md) — how to report vulnerabilities privately,
  and what is in scope.
- [Safety notes](../README.md#build-and-fabrication) — the Lamoka1/2 produces
  mains voltage. Never energize a board you built without completing the staged
  bring-up, and never leave a powered unit unattended.
- The DEV-Brain firmware (PIE V1.8) is bench-tested; the main-board firmware
  (PIE V1.9) compiles clean but has never run on hardware, and the power stage
  has never been energized — a freshly built Lamoka1/2 will not produce output
  in its current state.

## Contributing

- [Contributing guide](../CONTRIBUTING.md) — development setup, conventions,
  testing, and the pull-request checklist.
- [`Makefile`](../Makefile) — `make help` lists the common tasks: firmware
  build and upload, repository checks, linting, ERC/DRC reports, static
  analysis, STEP export, and regenerating the board pictures. The same checks
  run in CI on every push.
- [Code of Conduct](../CODE_OF_CONDUCT.md)
- [Changelog](../CHANGELOG.md)
