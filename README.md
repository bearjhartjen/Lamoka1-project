<div align="center">

<img src="Images/lamoka1-2-render-iso.png" alt="Lamoka1/2 main board" width="820">

[![RepoGrade](https://www.repo-grade.com/api/badge/bearjhartjen/lamoka1-project)](https://www.repo-grade.com/report/bearjhartjen/lamoka1-project)

</div>

# Lamoka1/2

The Lamoka1/2 is an open-source inverter. It takes 11–14 V DC from a battery and
turns it into **120 V RMS AC** — the flavor of power you get from a wall outlet.

This proect is about being intuitive,
accessible, and effective. Everything about it is open: the circuit board
files, the firmware, and the documentation. You can build one, repair one, or
change one however you'd like, and nothing about how it works is hidden. If you
don't have technical experience, that's completely alright — the stock firmware
gives you a full, evolving (we will maintain and update the project and firmware constantly), and working interface, and you can go under the hood if you
so choose.

Too many people in this world are trapped by closed, concealed innovation.
With the Lamoka1-project, we want to smash that barrier down, and put the control of
power back in the hands of the people. Flamingo and Cactus is managing
the Lamoka1-project to encourage better systems for the world.

## What's in this repository

This repository is the parent of two products. Both are real products in their
own right, and both are prototypes towards the completion of the Lamoka1 unit we are building.

| | What it is | Where it stands |
| --- | --- | --- |
| **Lamoka1/2** | The inverter: battery in, 120 V RMS AC out. This is our MVP and the test unit for the inverter-only Lamoka1. | Designed and laid out. The power stage has not been energised yet |
| **Lamoka1-DEV-Brain** | The control section on its own small board: the microcontroller, the button, and ten LEDs. A prototype of the Lamoka1's control side, and the board we develop/test stock firmware on. | Built, assembled, and bench-tested — a 72-hour burn-in test passed 

The Lamoka1/2 and Lamoka1-DEV-Brain are what Flamingo and Cactus will be live presenting at the Owego Rotary craft
fair: Elks Lodge, Owego, NY, November 14, 9am–3pm.

<p align="center">
  <img src="Images/lamoka1-2-render-top.png" alt="Lamoka1/2 top view" width="420">
  <img src="Images/lamoka1-2-render-bottom.png" alt="Lamoka1/2 bottom view" width="420">
  <br><em>The Lamoka1/2 as it is being constructed — green solder mask, white silkscreen.</em>
</p>

The Lamoka1/2 schematic went through the [Tomachie](https://tomachie.com) service to get the
topology checked; the report can be found 
[here](https://tomachie.com/r/0a968485-5479-41e2-b24f-00a293061b1e/Lamoka1-half_report.html#design-summary),
and a copy is kept in the repository under
[`HARDWARE/Kicadfiles/Lamoka1-half/Reviews/`](HARDWARE/Kicadfiles/Lamoka1-half/Reviews).

## Lamoka1/2 specifications

| | |
| --- | --- |
| **Input** | 11–14 V DC (for exmple a 12 V battery) |
| **Output** | 120 V RMS AC at the outlet |
| **How it works** | A boost converter raises the battery voltage to a 21.75 V rail, an H-bridge switches that into an AC waveform, and a transformer steps it up to 120 V RMS at the outlet |
| **Brain** | An RP2354A (Raspberry Pi) microcontroller |
| **Transformer** | Triad VPT24-6670, 160 VA, mounted off the board and wired into the main board|
| **Interface** | One button and five addressable RGB LEDs |
| **Cooling** | A small 5 V fan on the 5 V rail — runs whenever that rail is up; no speed control |
| **Monitoring** | Voltage sensing on input, after boost, and after H-bridge, Current sensing inline with input, thermal sensors for control and inversion sections |
| **Charging** | Not part of this model |

The full parts list is in [`HARDWARE/COMPONENTS.md`](HARDWARE/COMPONENTS.md)
(plain English, not to be used as a BOM), and the design intent is in
[`HARDWARE/Design_Notes.md`](HARDWARE/Design_Notes.md).

### The outlet voltage is an estimate — read this

> [!CAUTION]
> **Nothing after the transformer is measured.** The board watches the battery
> rail, the boost bus, the two bridge midpoints, the input current and two
> temperature sensors. It does **not** watch the 120 V side — there is no
> voltage or current sense on the output, and no way to know what the 115 V
> winding is doing without external metering.
>
> The firmware regulates a computed *estimate* of the outlet, using the
> transformer's catalogue numbers. Those numbers hold the output within around
> **3 % of 120.0 V RMS** across the load range on a transformer regulating
> 6–10 % (the catalogue value is 8 %).
>
> So please proceed with caution and consideration at all times.

## Build and fabrication

> [!WARNING]
> The Lamoka1/2 produces up to about 120 V RMS AC at its output — the same kind
> of voltage as a wall outlet. Treat the output as live at all times.
>  This is a development platform: the power
> stage has not been energised and its firmware (PIE V1.9) has not run on
> hardware.

## Firmware

The firmware is the **Portal Interface Engine (PIE)** — Lamoka's operating
system. One button drives everything: tap to scroll right, double-tap to go left, hold
to select.

- **PIE V1.8** — for the DEV-Brain. Bench-tested.
- **PIE V1.9** — the first Lamoka1/2 firmware. Five LEDs, temperature reads,
  and the power stage (explicit arming, PWM dead-time, soft start, closed-loop
  regulation, supervisory protection). Compiles clean, but it has not run on
  hardware.

[`SOFTWARE/README.md`](SOFTWARE/README.md) says which version runs on which
board, and
[`PIE-v1.8-information.md`](SOFTWARE/Portal%20Interface%20Engine/PIE-v1.8-information.md)
and
[`PIE-v1.9-information.md`](SOFTWARE/Portal%20Interface%20Engine/PIE-v1.9-information.md)
describe what each one does, gesture by gesture.

## Contributing

Help is welcome — hardware review, firmware, documentation, and testing all
move the project forward. Start with [CONTRIBUTING.md](CONTRIBUTING.md) for the
development setup, conventions, and pull-request checklist, and use the issue
templates for bug reports and feature requests. All participation is covered by
the [Code of Conduct](CODE_OF_CONDUCT.md), and security reports go through
[SECURITY.md](SECURITY.md).

```bash
# Clone the repository
git clone https://github.com/bearjhartjen/Lamoka1-project.git
cd Lamoka1-project

# Open the main board (Lamoka1/2) in KiCad 10
kicad "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pro"
```
Paste the commands above into your terminal to open up and edit our files 

## Licensing

- **Hardware** — CERN Open Hardware Licence v2, weakly reciprocal
  ([`LICENSE-HARD`](LICENSE-HARD); SPDX copy in
  [`LICENSES/CERN-OHL-W-2.0.txt`](LICENSES/CERN-OHL-W-2.0.txt)). You may build,
  modify, and sell units, provided modifications are shared back under the same
  licence.
- **Software** — GNU GPL v3.0 only ([`LICENSE-SOFT`](LICENSE-SOFT); SPDX copy in
  [`LICENSES/GPL-3.0-only.txt`](LICENSES/GPL-3.0-only.txt)). The firmware and
  tooling may be used, modified, and redistributed under the GPL; derivative
  software must remain GPL-3.0.
- Vendor datasheets and third-party 3D models are not redistributed here;
  download them from the manufacturer.

## More places to look

- [`HARDWARE/BRINGUP.md`](HARDWARE/BRINGUP.md) — the plain-language bring-up
  guide for both boards.
- [`SOFTWARE/`](SOFTWARE/) — the Portal Interface Engine (PIE) firmware; start
  with [`SOFTWARE/README.md`](SOFTWARE/README.md).
- [`HARDWARE/Kicadfiles/Lamoka1-half/`](HARDWARE/Kicadfiles/Lamoka1-half) — the
  Lamoka1/2 KiCad project.
- [`Lamoka1-DEV-Brain/`](Lamoka1-DEV-Brain) — the control board project, its
  test results, and how to order one.
- [`HARDWARE/Mechanical/`](HARDWARE/Mechanical) — STEP models of both boards
  for enclosure design.
- [`Images/`](Images) — green-mask board renders and the routing plot.
- [`docs/README.md`](docs/README.md) — the documentation index, for everything
  else.

## Usage of AI in this repository

This repository is checked, maintained, and updated by humans. The Lamoka
series is neither "vibe coded" nor blindly applied — thought, testing,
engineering judgment, and practical experience go into every decision.

AI is used as a tool to help us bridge the gap between our current resources
and the scale of what we want to accomplish. It may assist with research,
development, documentation, and other aspects of the project, but its output is
not treated as inherently correct. Human review, testing, and engineering
judgment remain essential to the development process.

PIE OS is an exception in that a significant portion of its code was generated
with the assistance of AI. However, it has been reviewed, tested extensively,
and refined to meet the same standards we apply to the rest of our work.

At Flamingo and Cactus, our goal is to provide the best products, services, and
experiences we can to as many people as possible. As a small team, there are
limits to what we can accomplish on our own. We have very large dreams, and AI
allows us to act as a much larger team without losing the human judgment and
responsibility behind what we create.


