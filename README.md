<div align="center">

<img src="Images/lamoka1-2-render-iso.png" alt="Lamoka1/2 main board" width="820">

[![RepoGrade](https://www.repo-grade.com/api/badge/bearjhartjen/lamoka1-project)](https://www.repo-grade.com/report/bearjhartjen/lamoka1-project)

</div>

# Lamoka1/2

The Lamoka1/2 is an open-source inverter. It takes 11–14 V DC from a battery and
turns it into **120 V RMS AC** — the flavor of power you get from a wall outlet.

It's not just about being open source though. It's also about being intuitive,
accessible, and effective. Everything about it is open: the circuit board
files, the firmware, and the documentation. You can build one, repair it, or
change it however you like, and nothing about how it works is hidden. If you
don't have technical experience, that's completely fine — the stock firmware
gives you a full, working interface, and you only go under the hood if you
want to.

Too many people in this world are trapped by closed, concealed innovation.
With the Lamoka1-project, we want to smash that barrier down, and put the control of
power back in the hands of the people. Flamingo and Cactus is managing
the Lamoka1-project

## What's in this repository

This repository is the parent of two products. Both are real products in their
own right, and both are prototypes of the Lamoka1 we are building towards.

| | What it is | Where it stands |
| --- | --- | --- |
| **Lamoka1/2** | The inverter: battery in, 120 V RMS AC out. This is our MVP and the test unit for the inverter-only Lamoka1. | Designed and laid out. The power stage has not been energised yet |
| **Lamoka1-DEV-Brain** | The control section on its own small board: the microcontroller, the button, and ten LEDs. A prototype of the Lamoka1's control side, and the board we develop the firmware on. | Built, assembled, and bench-tested — a 72-hour burn-in passed |

The Lamoka1/2 is what Flamingo and Cactus are showing at the Owego Rotary craft
fair: Elks Lodge, Owego, NY, November 14, 9am–3pm.

<p align="center">
  <img src="Images/lamoka1-2-render-top.png" alt="Lamoka1/2 top view" width="420">
  <img src="Images/lamoka1-2-render-bottom.png" alt="Lamoka1/2 bottom view" width="420">
  <br><em>The Lamoka1/2 as it is being built — green solder mask, white legend.</em>
</p>

The Lamoka1/2 board went through [Tomachie](https://tomachie.com) to get the
design checked; the report is
[here](https://tomachie.com/r/0a968485-5479-41e2-b24f-00a293061b1e/Lamoka1-half_report.html#design-summary),
and a copy is kept in the repository under
[`HARDWARE/Kicadfiles/Lamoka1-half/Reviews/`](HARDWARE/Kicadfiles/Lamoka1-half/Reviews).

## Specifications

| | |
| --- | --- |
| **Input** | 11–14 V DC (a 12 V battery) |
| **Output** | 120 V RMS AC at the outlet |
| **How it works** | A boost converter raises the battery voltage to a 21.75 V bus, an H-bridge switches that into an AC waveform, and a transformer steps it up to 120 V RMS at the outlet |
| **Brain** | An RP2354A (Raspberry Pi) microcontroller |
| **Transformer** | Triad VPT24-6670, 160 VA, mounted off the board and wired to it |
| **Interface** | One button and addressable RGB LEDs — five on the Lamoka1/2, ten on the DEV-Brain |
| **Cooling** | A small 5 V fan on the 5 V rail — runs whenever that rail is up; no speed control |
| **Monitoring** | Input, bus, and bridge voltages; load current; two temperature sensors |
| **Charging** | Not part of this model |

The full parts list is in [`HARDWARE/COMPONENTS.md`](HARDWARE/COMPONENTS.md)
(plain English, and not the BOM), and the design intent is in
[`HARDWARE/Design_Notes.md`](HARDWARE/Design_Notes.md).

### The outlet voltage is an estimate — read this

> [!CAUTION]
> **Nothing after the transformer is measured.** The board watches the battery
> rail, the boost bus, the two bridge midpoints, the input current and two
> temperature sensors. It does **not** watch the 120 V side — there is no
> voltage or current sense on the output, and no way to know what the 115 V
> winding is doing except with a meter.
>
> The firmware regulates a computed *estimate* of the outlet, using the
> transformer's catalogue numbers. Those numbers hold the output within about
> **3 % of 120.0 V RMS** across the load range on a transformer regulating
> 6–10 % (the catalogue value is 8 %), and no unit needs to be calibrated.
> That is a good estimate — but it is still an estimate.
>
> So please proceed with caution and consideration: treat the output as live
> mains, measure it yourself the first few times you power one up, and never
> leave a powered unit unattended.

A later Lamoka1 model will add a direct isolated output sense and measure the
outlet properly. This one does not have it.

## Getting started

The repository holds the design files (KiCad 10), the firmware (Arduino), and
the documentation.

```bash
# Clone the repository
git clone https://github.com/bearjhartjen/Lamoka1-project.git
cd Lamoka1-project

# Open the main board (Lamoka1/2) in KiCad 10
kicad "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pro"

# Compile the firmware — the default is the bench-tested DEV-Brain head,
# PIE V1.8 (needs arduino-cli; see Build and fabrication)
make firmware
```

The [`Makefile`](Makefile) wraps the common tasks — `make help` lists them all.

### The Lamoka1-DEV-Brain comes first

The DEV-Brain is the board you can actually build today, and it is where the
firmware gets developed. It is a product in its own right, not a throwaway
test jig.

- **[Lamoka1-DEV-Brain guide](Lamoka1-DEV-Brain/Readme.md)** — what the board
  is, how to order one from PCBWay, and how to put the firmware on it.
- **[Bring-up guide](HARDWARE/BRINGUP.md)** — the whole process in plain
  language: the DEV-Brain first, then the Lamoka1/2, one stage at a time.

## Build and fabrication

> [!WARNING]
> The Lamoka1/2 produces up to about 120 V RMS AC at its output — the same kind
> of voltage as a wall outlet. Treat the output as live at all times, and never
> leave a powered unit unattended. This is a development platform: the power
> stage has not been energised and its firmware (PIE V1.9) has not run on
> hardware, so a freshly built board will not produce output yet.

### 1. Order the PCB

The ready-to-order package — Gerbers, drill file, BOM, and pick-and-place — is
committed under
[`HARDWARE/Lamoka1-half fab files/`](HARDWARE/Lamoka1-half%20fab%20files/README.md),
together with notes on what to check when ordering. The board is 2 layers,
1.6 mm FR-4, green solder mask with white legend, and 95.2 × 95.2 mm. The
folder also carries an
[interactive BOM](HARDWARE/Lamoka1-half%20fab%20files/Lamoka1-half%20interactive%20BOM.html)
for placing and checking parts.

<p align="center">
  <a href="https://htmlpreview.github.io/?https://raw.githubusercontent.com/bearjhartjen/Lamoka1-project/Lamoka1/HARDWARE/Kicadfiles/Lamoka1-half/BOM/Lamoka1-half.html">
    <strong>View Interactive BOM</strong>
  </a>
</p>

To export the Gerbers and drill file yourself:

```bash
mkdir -p ~/lamoka-fab
kicad-cli pcb export gerbers --output ~/lamoka-fab/ "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pcb"
kicad-cli pcb export drill   --output ~/lamoka-fab/ "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pcb"
```

In KiCad, the same exports are under **File → Fabrication Outputs**.

### 2. Order the parts

The orderable bill of materials is
[`HARDWARE/Lamoka1-half fab files/Lamoka1-half BOM.csv`](HARDWARE/Lamoka1-half%20fab%20files/Lamoka1-half%20BOM.csv).
It is generated from the schematic by
[`tools/make_fab_bom.py`](tools/make_fab_bom.py) — rerun it after design
changes. The large off-board items (transformer, fan, outlet, Wago connectors)
are listed in [`HARDWARE/COMPONENTS.md`](HARDWARE/COMPONENTS.md).

### 3. Assemble the board

SMD parts are fitted on both sides, so plan a two-pass reflow (or use a stencil
and a hot plate), then hand-solder the through-hole parts: the Wago terminals,
the button, the USB-C shield, and the TO-220 MOSFETs. Check for solder bridges
and shorts with a multimeter before applying power.

### 4. Bring it up

The full, step-by-step procedure lives in
**[`HARDWARE/BRINGUP.md`](HARDWARE/BRINGUP.md)** — the DEV-Brain first, then
the Lamoka1/2 in stages, with what to expect at every step. Read it before
you plug anything in.

The short version: check the board over, bring it up on USB-C alone, then from
a current-limited bench supply, check the gate drive, and only then connect the
transformer and the outlet. Read
[`HARDWARE/Transformer-wiring.md`](HARDWARE/Transformer-wiring.md) before
wiring the transformer — the `J1` net names are lead colours, so the
transformer leads must **not** be fitted colour-for-colour.

### 5. Flash the firmware

PIE V1.8 — the bench-tested one — runs on the DEV-Brain. It is built with the
[arduino-pico](https://github.com/earlephilhower/arduino-pico) core and the
[FastLED](https://github.com/FastLED/FastLED) library.

With the Arduino IDE: add the board manager URL
`https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
under **File → Preferences → Additional Boards Manager URLs**, install the
**Raspberry Pi Pico/RP2040/RP2350** core and **FastLED**, then open
[`V1.8.ino`](SOFTWARE/Portal%20Interface%20Engine/V1.8.ino) and upload.

Or with the Makefile and `arduino-cli`:

```bash
arduino-cli core install rp2040:rp2040 \
  --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli lib install FastLED

make firmware                    # compile the default (DEV-Brain head, V1.8)
make firmware SKETCH="SOFTWARE/Portal Interface Engine/V1.9.ino"   # main-board head
make upload PORT=/dev/ttyACM0    # compile and flash
```

If the port does not appear, put the board into USB bootloader mode from its
own UI: **Settings → BOOTSEL**. The DEV-Brain has no BOOTSEL button of its own.

## Firmware

The firmware is the **Portal Interface Engine (PIE)** — Lamoka's operating
system. One button drives everything: tap to move, double-tap to go back, hold
to select. It runs the lights (status ball, fireplace, lightsaber, rainbow), a
small "Fireball" game, settings, sleep, and USB bootloader entry; on the
Lamoka1/2 it also runs the power stage.

We think the control and logic side of the Lamoka is where we get to expose
our creativity and our community for free, and to just truly have fun. PIE is
how the unit talks back to you — and it is the part we are most proud of.
The repository keeps two heads, both maintained in place; git history holds
the earlier releases:

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

## Contributing

Help is welcome — hardware review, firmware, documentation, and testing all
move the project forward. Start with [CONTRIBUTING.md](CONTRIBUTING.md) for the
development setup, conventions, and pull-request checklist, and use the issue
templates for bug reports and feature requests. All participation is covered by
the [Code of Conduct](CODE_OF_CONDUCT.md), and security reports go through
[SECURITY.md](SECURITY.md).

## License

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
