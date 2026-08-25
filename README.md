# Lamoka1 - Autonomous Power Inverter

## What is Lamoka1?
The Lamoka1 is a fully open-source, pure sine wave inverter and charger system controlled by an RP2354A microcontroller. It's designed to be modular, repairable, and fully configurable through user code.

## Repository Structure

```
Lamoka1-project/
├── HARDWARE/           # Main hardware design files
│   ├── kicadfiles/     # KiCad project (schematics, PCB, libs)
│   ├── 3D_models/      # Enclosure and component 3D models
│   └── BOM/            # Bill of Materials
├── SOFTWARE/           # Firmware source code
│   └── src/            # RP2354A C/C++ firmware
├── Lamoka1-DEV-BRAIN/  # Development testing board
├── docs/               # Project documentation
│   ├── ARCHITECTURE.md # System architecture overview
│   ├── SCHEMATIC.md    # Schematic organization
│   └── DESIGN.md       # Design decisions log
├── Images/             # Diagrams, photos, renders
└── LICENSE-*           # Hardware/Firmware licenses
```

## Key Features

- **Pure sine wave output** - Up to 180V peak
- **Modular design** - Replaceable, well-documented sections
- **Fully configurable** - RP2354A control via I2C
- **Adaptive cooling** - PWM-controlled fan
- **Solar ready** - MPPT charging support

## Development Progress

### Architecture Status
- ✅ Two-stage topology verified
- ✅ 48V boost stage feasible (LM5122, 70-79% duty)
- ✅ Flyback isolation feasible (Ns/Np=4.2)
- ✅ H-bridge modulation verified (M=0.85)
- 🚧 Component selection in progress
- 🚧 PCB layout pending

### Power Flow
```
Input: 10-14.6V LiFePO4 (≤30A, 300-438W)
→ 48V Boost Stage (93% efficiency)
→ 200V Isolated Bus (89% efficiency)
→ H-Bridge Inverter (92% efficiency)
= 120VAC Output (82% total efficiency)
```