<div align="center">

<img src="Images/lamoka1-2-render-iso.png" alt="Lamoka1/2 main PCB" width="820">

<br>

<a href="https://www.bestpractices.dev/projects/14750">
  <img src="https://www.bestpractices.dev/projects/14750/badge" alt="OpenSSF Best Practices" height="22">
</a>
&nbsp;&nbsp;
<a href="https://www.repo-grade.com/report/bearjhartjen/lamoka1-project">
  <img src="https://www.repo-grade.com/api/badge/bearjhartjen/lamoka1-project" alt="RepoGrade" height="22">
</a>

</div>

---

## Owego Rotary craft fair

For the Owego Rotary craft fair, taking place at the Elks Lodge in Owego, NY, on November 14th from 9am to 3pm, the main project being presented by Flamingo and Cactus will be the Lamoka1/2, an inverter-only version of the Lamoka1. More details will be presented after the event.


## What is the Lamoka1?

The Lamoka1 is an open-source inverter/charger system designed for complete transparency and control. Unlike market alternatives that are locked-down black boxes, the Lamoka1 gives you full access to your power conversion. If you don't have technical experience, that's completely fine—our Portal Interface Engine (Lamoka's operating system) provides a solid stock setup. But if you want to get technical, feel free to customize and modify the unit however you like.

---

## Specifications

The Lamoka1 inverter section takes an 11V–14V input, steps it up to a 17V rail via an LM5122 boost converter, and feeds it into an H-bridge (four CSD19536KCS NMOSs driven by two UCC27735DR gate drivers) controlled by an RP2354A microcontroller—giving you total flexibility to program whatever waveform or output you need. From there, it passes into a VPT24-4170 transformer to step up the voltage.

We use a Qualtek FAD1-04010BHLW11 fan powered from the 5V buck rail for cooling. GPIO5 and GPIO6 are unused, so the MCU provides no PWM speed control or tach feedback for the fan. The two TMP235A2DBZR temperature sensors are monitored by the system. Voltage and current monitoring is handled by the ADS7128IRTER multiplexer, the 5V rail by the TPS62160DSGR buck converter, and the user interface by addressable WS2812B LEDs with a B3F-4055 button (five LEDs on the Lamoka1/2 main board; the DEV-Brain control board carries ten).

---

## Quick start

The repository is source-first: KiCad 10 projects, Arduino firmware, and design documents. You can open the design with nothing but KiCad installed, and build the current firmware for the Lamoka1-DEV-Brain control board.

```bash
# 1. Clone the repository
git clone https://github.com/bearjhartjen/Lamoka1-project.git
cd Lamoka1-project

# 2. Open the main board (inverter-only Lamoka1/2) in KiCad 10
#    (or open HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_pro from the KiCad GUI)
kicad "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_pro"

# 3. Export a fabrication package for your own build
mkdir -p ~/lamoka-fab
kicad-cli pcb export gerbers --output ~/lamoka-fab/ "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_pcb"
kicad-cli pcb export drill   --output ~/lamoka-fab/ "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_pcb"
```

The repository also ships a [`Makefile`](Makefile) that wraps the common tasks
(`make help` lists them): `make firmware` builds the current PIE sketch for the
DEV-Brain, `make test` runs the repository checks, `make check` adds the KiCad
ERC and DRC reports, and `make static` runs cppcheck over the firmware.

The Portal Interface Engine (PIE) firmware drives the ten WS2812B LEDs and the single button on the Lamoka1-DEV-Brain. Every revision is a separate sketch file, and V1.8 is the current head:

```cpp
// SOFTWARE/Portal Interface Engine/V1.8.ino (excerpt) — targets the Lamoka1-DEV-Brain
#define LED_PIN    11   // WS2812B data
#define NUM_LEDS   10   // DEV-Brain fits ten LEDs (D1-D10)
#define BUTTON_PIN  0   // single-button UI, INPUT_PULLUP

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  // ...
}
```

One button drives the whole interface: tap to move, double-tap to go back, hold to select. The full gesture map is in [PIE-v1.8-information.md](SOFTWARE/Portal%20Interface%20Engine/PIE-v1.8-information.md). Flashing instructions are in [Build and fabrication](#build-and-fabrication).

---

## Hardware

| Board | What it is | State | Design files |
| --- | --- | --- | --- |
| **Lamoka1/2** (main) | Inverter board: 11–14V input, 17V boost, H-bridge, RP2354A, output transformer wired off-board | Laid out; power stage never energized | [`HARDWARE/Kicadfiles/Lamoka1/`](HARDWARE/Kicadfiles/Lamoka1) |
| **Lamoka1-DEV-Brain** | Control-only board (MCU, LEDs, button) used to develop the firmware | Built and assembled; 72-hour burn-in passed | [`Lamoka1-DEV-Brain/`](Lamoka1-DEV-Brain) |

<p align="center">
  <img src="Images/lamoka1-2-render-top.png" alt="Lamoka1/2 top view" width="420">
  <img src="Images/lamoka1-2-render-bottom.png" alt="Lamoka1/2 bottom view" width="420">
  <br><em>3D renders of the current layout — exported with <code>kicad-cli pcb render</code>.</em>
</p>

<p align="center">
  <img src="Images/lamoka1-2-routing-copper.png" alt="Copper layers: F.Cu in red, B.Cu in blue" width="560">
  <br><em>Copper layers — F.Cu (red) and B.Cu (blue), plotted from the KiCad layout.</em>
</p>

Design documentation lives alongside the sources:

- [Design_Notes.md](HARDWARE/Design_Notes.md) — design intent, pin map, H-bridge and monitoring details.
- [COMPONENTS.md](HARDWARE/COMPONENTS.md) — plain-English component reference (not the BOM).
- [SOFTWARE/Operation.md](SOFTWARE/Operation.md) — intended boot sequence and behavior.
- [Lamoka1-DEV-Brain/Readme.md](Lamoka1-DEV-Brain/Readme.md) — the control board, its test results, and how to order it.
- [docs/README.md](docs/README.md) — documentation index for the whole project.

---

## Build and fabrication

> [!WARNING]
> The Lamoka1/2 produces mains voltage (120V AC, North American outlet). Treat the output as live at all times and never leave a powered unit unattended. The power stage has been laid out but has never been energized, and power-stage control firmware does not exist yet — this revision is a development platform, not a finished product.

### 1. Order the PCB

Export the fabrication package straight from the KiCad project. The board is **2 layers, 1.6 mm FR-4, 95.0 × 95.0 mm**, with 0.15 mm minimum tracks, 0.18 mm minimum clearance, and 0.2032 mm minimum drills — any fab that quotes 6 mil features can build it (JLCPCB, PCBWay, OSH Park, …).

```bash
# Gerbers, drill files and (optionally) a pick-and-place file
mkdir -p ~/lamoka-fab
kicad-cli pcb export gerbers --output ~/lamoka-fab/ "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_pcb"
kicad-cli pcb export drill   --output ~/lamoka-fab/ "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_pcb"
kicad-cli pcb export pos --format csv --units mm \
  --output ~/lamoka-fab/Lamoka1-pos.csv "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_pcb"
```

In the KiCad GUI the same output is under **File → Fabrication Outputs → Gerbers / Drill Files**. Zip `~/lamoka-fab/` and upload it to your fab of choice. The DEV-Brain was manufactured and assembled by [PCBWay](https://www.pcbway.com/project/shareproject/Lamoka1_DEV_Brain_49e4fd0a.html); its fab files (including the exact package we ordered) are committed under `Lamoka1-DEV-Brain/Lamoka1-DEV-Brain fab files/` as a worked example.

### 2. Order the parts

The bill of materials generated from the schematic is committed as [`HARDWARE/Kicadfiles/Lamoka1/Lamoka1.csv`](HARDWARE/Kicadfiles/Lamoka1/Lamoka1.csv). It carries Reference, Quantity, Value, Footprint, MPN and Description columns; the schematic remains authoritative, and the BOM export leaves the Manufacturer column blank on most lines, so add manufacturer or distributor order codes before you order. The large items (transformer, fan, outlet, Wago connectors) are listed in [COMPONENTS.md](HARDWARE/COMPONENTS.md); we sourced them from DigiKey.

Regenerate the BOM after design changes from the KiCad GUI (**Tools → Generate BOM**) or the command line:

```bash
kicad-cli sch export bom \
  --fields "Reference,QUANTITY,Value,Footprint,MPN,Description" \
  --labels "Reference,Qty,Value,Footprint,MPN,Description" \
  --group-by "Value,Footprint,MPN" \
  --output "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.csv" \
  "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_sch"
```

### 3. Assemble the board

SMD parts are fitted on both sides, so plan a two-pass reflow or use a stencil and a hot plate; then hand-solder the through-hole parts. Rough order of operations:

1. Stencil solder paste onto both sides (a thin 4 mil stencil works for the 0402/0603 parts).
2. Place the SMDs and reflow. Populate the front side first, then the back.
3. Hand-solder the through-hole parts: the Wago connectors, the fan header, the electrolytic capacitors, and the TO-220 H-bridge MOSFETs (Q3–Q6), which stand vertically along the board edge and can carry a heatsink.
4. Check for solder bridges and shorts with a multimeter before applying power — especially on the 11–14V input and the 17V boost rail.

### 4. Bring-up

The transformer, DC input, and the AC outlet wire into the Wago connectors on the board edge (see [Design_Notes.md](HARDWARE/Design_Notes.md)). Bring-up is done in stages, and low-voltage checks come before anything touches the transformer:

1. **Visual + continuity check** — no short between the input rails and ground.
2. **USB-C only (5V)** — confirm the 5V buck and 3.3V rails come up.
3. **11–14V from a current-limited bench supply** (start around half an amp) — confirm the 17V boost rail and check for abnormal current draw.
4. **Gate-drive check** — verify the UCC27735 outputs at the MOSFET gates before the transformer is connected.
5. **Transformer and output last** — connect the transformer and the outlet only after the low-voltage side checks out, and treat the output as live AC from that point on.

> [!NOTE]
> Power-stage control firmware has not been written yet: PIE V1.8 is DEV-Brain firmware for the LEDs and button, not inverter control. Don't expect the board to produce output in its current state.

### 5. Flash the firmware (Lamoka1-DEV-Brain)

PIE V1.8 runs on the DEV-Brain and is built with the [arduino-pico](https://github.com/earlephilhower/arduino-pico) core (RP2350 support) and the FastLED library.

With the Arduino IDE:

1. Add `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json` under **File → Preferences → Additional Boards Manager URLs**, then install the **Raspberry Pi Pico/RP2040/RP2350** core in Boards Manager.
2. Install **FastLED** from Library Manager.
3. Open [`SOFTWARE/Portal Interface Engine/V1.8.ino`](SOFTWARE/Portal%20Interface%20Engine/V1.8.ino), select your RP2350-family board (e.g. *Generic RP2350* for a bare RP2354A) and the board's USB port, and click **Upload**.

With `arduino-cli` — or `make upload PORT=/dev/ttyACM0`, which stages the
sketch and does both steps for you:

```bash
arduino-cli core install rp2040:rp2040 \
  --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli lib install FastLED

make firmware                    # compile the current revision
make upload PORT=/dev/ttyACM0    # compile and flash
```

Arduino expects each sketch to live in a folder of the same name, and the PIE
revisions deliberately share one folder — `make firmware` copies the chosen
revision into its own folder under `build/` before compiling, so nothing has to
be renamed or moved by hand.

If the port does not appear, put the board into USB bootloader mode: select **Settings → BOOTSEL** in the UI (a very long hold also works), or hold the button down while plugging in USB.

### 6. Mechanical model

The KiCad layout exports to a STEP model at [`HARDWARE/Mechanical/Lamoka1.step`](HARDWARE/Mechanical/Lamoka1.step) (board outline, holes, and component bodies) for enclosure design and fit checks. Regenerate it after layout changes with:

```bash
kicad-cli pcb export step \
  --output "HARDWARE/Mechanical/Lamoka1.step" "HARDWARE/Kicadfiles/Lamoka1/Lamoka1.kicad_pcb"
```

---

## Firmware

The Portal Interface Engine (PIE) is Lamoka's operating-system firmware. It is a one-button LED interface (status ball, fireplace, lightsaber, rainbow, the Fireball game, settings, sleep, USB BOOTSEL) written as Arduino sketches with `#define` pin constants, FastLED for the LEDs, and `pico/bootrom.h` for bootloader entry. V1.8 is the current head, and each revision is a new file — see [SOFTWARE/README.md](SOFTWARE/README.md) for board compatibility and [PIE-v1.8-information.md](SOFTWARE/Portal%20Interface%20Engine/PIE-v1.8-information.md) for the full behavioral description.

---

## Contributing

Contributions are welcome — hardware review, firmware, documentation, and testing all help. Start with [CONTRIBUTING.md](CONTRIBUTING.md) for the development setup, KiCad and firmware conventions, and the pull-request checklist, then use the issue templates to report bugs or request features. All participation is covered by our [Code of Conduct](CODE_OF_CONDUCT.md); security reports go through [SECURITY.md](SECURITY.md).

---

## License

- **Hardware** — CERN Open Hardware Licence v2, weakly reciprocal ([`LICENSE-HARD`](LICENSE-HARD); SPDX-named copy in [`LICENSES/CERN-OHL-W-2.0.txt`](LICENSES/CERN-OHL-W-2.0.txt)). You may build, modify, and sell units, provided modifications are shared back under the same licence.
- **Software** — GNU GPL v3.0 only ([`LICENSE-SOFT`](LICENSE-SOFT); SPDX-named copy in [`LICENSES/GPL-3.0-only.txt`](LICENSES/GPL-3.0-only.txt)). The firmware and tooling may be used, modified, and redistributed under the GPL; derivative software must remain GPL-3.0.
- Vendor datasheets and third-party 3D models are not redistributed here; download them from the manufacturer.

---

## Usage of AI in this repository


This repository is checked, maintained, and updated by humans. The Lamoka series is neither “vibe coded” nor blindly applied. Thought, testing, engineering judgment, and practical experience go into every decision made throughout the project.

AI is used as a tool to help us bridge the gap between our current resources and the scale of what we want to accomplish. It may assist with research, development, documentation, and other aspects of the project, but its output is not treated as inherently correct. Human review, testing, and engineering judgment remain essential to the development process.

PIE OS is an exception in that a significant portion of its code was generated with the assistance of AI. However, it has been reviewed, tested extensively, and refined to meet the same standards we apply to the rest of our work.

At Flamingo and Cactus, our goal is to provide the best products, services, and experiences we can to as many people as possible. As a small team, there are limits to what we can accomplish on our own. We have very large dreams, and AI allows us to act as a much larger team without losing the human judgment and responsibility behind what we create.
