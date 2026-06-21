


[![Status](https://img.shields.io/badge/status-early%20design-yellow.svg)]()
[![Hardware License](https://img.shields.io/badge/Hardware-CERN--OHL--W%20v2.0-orange.svg)](./lamoka1hardware/LICENSE-HARD)
[![Firmware License](https://img.shields.io/badge/Firmware-GPLv3-blue.svg)](./Lamoka1software/LICENSE-SOFT)
[![Ecosystem](https://img.shields.io/badge/ecosystem-Lamoka-ff69b4.svg)](../)

</div>

---

## 📋 Table of Contents

- [Overview](#-overview)
- [Key Features](#-key-features)
- [Specifications](#-specifications)
- [Operating Modes](#-operating-modes)
- [Software-Defined Control](#-software-defined-control)
- [License](#-license)
- [Contributing](#-contributing)

---

## 🔋 Overview

**Lamoka1** is the first product in the Lamoka ecosystem — a fully open-source, pure sine wave inverter and charger system, controlled by an RP2350. It's built to be fully transparent, modular, and repairable, aiming to be the best in its class rather than a closed, disposable black box. Every schematic, every line of firmware, and every design decision is published — nothing is hidden behind proprietary lock-in.

It's designed for solar enthusiasts, electrical engineers, and everyday users alike, with a simple physical interface hiding a fully configurable system underneath.

> [!NOTE]
> This is an early-stage hardware project — the boost converter topology is still being finalized.

---

## ✨ Key Features

- 🌊 **Pure sine wave output** — or virtually any waveform you want, up to a 180V peak limit
- 🔧 **Modular & repairable** — built from replaceable, well-documented sections instead of a sealed, disposable unit
- ⚙️ **Fully configurable** — every behavior can be customized through user code, not locked behind firmware
- 🔌 **Dual operating modes** — standard sine-wave operation, or a stripped-down "dumb mode" for purely resistive loads (see below)
- 🌡️ **Adaptive cooling** — per-section temperature sensing drives a PWM-controlled fan, scaling speed precisely instead of crude on/off switching
- 💡 **Minimal UI** — ten RGB LEDs and a single push button handle the entire physical interface

---

## 📐 Specifications

| Parameter | Value |
|:---|:---|
| **DC Input** | 5–40V |
| **AC Output** | 120V RMS |
| **Peak Output Voltage** | 180V peak (configurable waveform) |
| **Control** | RP2350 microcontroller |
| **User Interface** | 10× RGB LEDs + 1 push button |
| **Cooling** | PWM-controlled fan, per-section temperature sensing |

---

## 🎛️ Operating Modes

### Standard Mode
Produces a true sine wave (or any custom waveform within the 180V peak limit) via the H-bridge — ideal for sensitive electronics and general use.

### Dumb Mode
A simplified mode intended **only for purely resistive loads**. Instead of synthesizing a sine wave, the H-bridge generates simple square pulses, using an adjusted Vrms calculation to land on 120V output. One leg of the H-bridge handles the entire waveform — switching directly between positive and negative — rather than splitting the duty cycle across both halves.

> [!TIP]
> Dumb mode trades waveform purity for simplicity and efficiency — don't use it with anything other than purely resistive loads (e.g. resistive heating elements).

---

## 🧠 Software-Defined Control

The entire H-bridge is driven directly by the RP2350 — there's no separate analog waveform generator or fixed-function inverter chip in the way. Every switching decision, every waveform shape, every mode is just code running on the microcontroller.

Out of the box, Lamoka1 ships with **basic stock firmware**: clean sine wave output and the resistive-load "dumb mode" described above. That's intentionally a starting point, not a ceiling.

> [!TIP]
> Because the RP2350 controls switching directly, the unit can be reprogrammed to do far more than the stock firmware — custom waveforms, different control strategies, alternative protection logic, whatever you can get the math to do safely. This is exactly where the open-source firmware license matters: if you build something cool, you can share it back with the community, and anyone else with a Lamoka1 can run it.

---

## ⚖️ License

Lamoka1 uses a split license, matching the folder you're in:

- **Hardware** (`lamoka1hardware/`) — Licensed under [CERN-OHL-W v2.0](https://ohwr.org/cern_ohl_w_v2.txt), full text in [`LICENSE-HARD`](./lamoka1hardware/LICENSE-HARD)
- **Firmware** (`Lamoka1software/`) — Licensed under [GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html), full text in [`LICENSE-SOFT`](./Lamoka1software/LICENSE-SOFT)

---

## 🤝 Contributing

Want to help build Lamoka1? See the [contributing guide](../CONTRIBUTING.md) for how to get involved, including the per-folder `ADDITION.md` requirement for tracking contributions.

