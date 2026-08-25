# HARDWARE Design Files

## Project Structure

```
HARDWARE/
├── kicadfiles/       # Main KiCad project files
│   ├── Lamoka1.kicad_pro  # Project file
│   ├── Lamoka1.kicad_sch    # Top-level schematic
│   ├── Lamoka1.kicad_pcb    # PCB layout
│   ├── Lamoka1-Boost.kicad_sch  # Boost converter
│   └── sym-lib-table          # Symbol library
├── 3D_models/        # 3D CAD files
├── BOM/              # Bill of Materials
│   └── BOM.csv      # Component list
└── notes/            # Design notes and calculations
    └── DESIGN.md    # Key design decisions
```

## Schematics

| File | Section | Status |
|------|---------|--------|
| Lamoka1.kicad_sch | System overview | ✅ Base sheet |
| Lamoka1-Boost.kicad_sch | 48V boost stage | ✅ Verified |
| ChargingSection.kicad_sch | Battery charger | ✅ Complete |
| Control.kicad_sch | Control logic | ✅ Complete |

## PCB Layout

- **Layers**: 4-layer stackup
- **Min clearances**: 0.2mm
- **High voltage**: 3mm creepage
- **Current paths**: 4oz copper for >20A traces

## Bill of Materials

Full BOM available in `HARDWARE/BOM/BOM.csv`

### Critical Components

| Ref | Part | Qty | Status |
|-----|------|-----|--------|
| U1 | LM5122 | 1 | ✅ Available |
| Q1-Q6 | 300V MOSFET | 6 | 🚧 Selected: IRF730 |
| T1 | EFD30 Transformer | 1 | 🚧 Design needed |
| C1-C10 | Bulk caps | 10 | ✅ Available |