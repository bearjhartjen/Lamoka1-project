# Documentation index

This folder is the door into the Lamoka1 documentation. The authoritative
sources live next to the design files and firmware they describe — the KiCad
schematics, the firmware, and the reports are the specification — and this page
links to them. Nothing about how the Lamoka works is hidden; you just have to
know where to look.

## Getting started

- [Getting started](../README.md#getting-started) — clone the repository, open
  the KiCad project, and build the firmware.
- [Bring-up guide](../HARDWARE/BRINGUP.md) — the whole process in plain
  language: the DEV-Brain first, then the Lamoka1/2, one stage at a time.
- [Build and fabrication](../README.md#build-and-fabrication) — ordering the
  PCB, ordering the parts, assembling the board, and flashing the firmware.

## Using the project

- [Specifications](../README.md#specifications) — what the Lamoka1/2 does.
- [The outlet voltage is an estimate](../README.md#the-outlet-voltage-is-an-estimate--read-this)
  — nothing after the transformer is measured. Read this before you power one
  up.
- [Operation guide](../SOFTWARE/Operation.md) — how the Lamoka1/2 boots.
- [PIE v1.8 information](../SOFTWARE/Portal%20Interface%20Engine/PIE-v1.8-information.md)
  — button gestures, UI states, brightness and speed tables, and timings of the
  DEV-Brain firmware.
- [PIE v1.9 information](../SOFTWARE/Portal%20Interface%20Engine/PIE-v1.9-information.md)
  — the main-board firmware: the five-LED interface plus arming, PWM dead-time,
  soft start, closed-loop regulation, and supervisory protection. The output
  target is 120.0 V RMS at the outlet on a 21.75 V bus, regulated from a
  droop-compensated estimate of the outlet rather than a direct measurement.
- [Lamoka1-DEV-Brain guide](../Lamoka1-DEV-Brain/Readme.md) — the control
  board, how to order one, and how to get the firmware onto it.

## Hardware design

- [Design notes](../HARDWARE/Design_Notes.md) — design intent and rationale.
- [Transformer wiring](../HARDWARE/Transformer-wiring.md) — the `J1` lead map.
  Read it before wiring: the net names are lead colours.
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
  mains voltage. Never energize a board you built without completing the
  staged bring-up, and never leave a powered unit unattended.
- The DEV-Brain firmware (PIE V1.8) is bench-tested; the main-board firmware
  (PIE V1.9) compiles clean but has not run on hardware, and the power stage
  has not been energized — a freshly built Lamoka1/2 will not produce output in
  its current state.
- **Nothing after the transformer is measured.** There is no voltage or current
  sensing on the 115 V side, so the firmware regulates an estimate of the
  outlet. Measure it yourself with a meter.
- The outlet is a 120 V RMS mains socket. Its earth pole is not the board's
  ground: the harness has to provide the protective-earth bond (or the unit
  has to be marked double-insulated), and a three-prong outlet must never be
  shipped with its earth left floating or tied to logic ground. See
  [`HARDWARE/Transformer-wiring.md`](../HARDWARE/Transformer-wiring.md).

## Contributing

- [Contributing guide](../CONTRIBUTING.md) — development setup, conventions,
  testing, and the pull-request checklist.
- [`Makefile`](../Makefile) — `make help` lists the common tasks: firmware
  build and upload, repository checks, linting, ERC/DRC reports, static
  analysis, STEP export, and regenerating the board pictures. The same checks
  run in CI on every push.
- [Code of Conduct](../CODE_OF_CONDUCT.md)
- [Changelog](../CHANGELOG.md)
