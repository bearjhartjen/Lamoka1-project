# Inverter Section Design - To Be Added to Lamoka1 Schematics

## Overview
The inverter section converts DC voltage to pure sine wave AC output (120Vrms nominal, up to 180V peak).

## Missing Components

### 1. H-Bridge Inverter Topology
```
               VDC Bus (~340V for 120Vrms)
                    |
           ┌─────┬─┴─┬─────┐
           │     │     │     │
          Q1     Q2    Q3     Q4
         (H1)   (L1)  (H2)   (L2)
           │     │     │     │
           └─────┬─┬─────┘
                 │ │
            L1   │ │   L2
                 │ │
                 └──┴──→ AC Output (L1-N for single phase)
                 │ │
       DC Link    └─┘   Output Filter
    (340V+) ──────────── LC Filter ───────────→ 120Vrms AC
```

### 2. Required Components

| Component | Value | Part Number | Qty | Notes |
|-----------|-------|-------------|-----|-------|
| MOSFETs | 200V, 10A+ | IRF730/IRF740 or similar | 4 | N-channel for H-bridge |
| Gate Drivers | Isolated 200V | ADuM4225 or similar | 4 | For safe gate driving |
| DC Link Cap | 470µF, 450V | Panasonic EEU-XXSeries | 2 | High voltage electrolytic |
| DC Link Cap | 0.1µF, 400V | ceramic NPO | 4 | High frequency decoupling |
| Inductor (filter) | 1mH, 2A | Power inductor | 1-2 | Output LC filter |
| Capacitor (filter) | 10µF, 400V | Film capacitor | 2 | Output LC filter |
| Fuses | 5A slow-blow | Littelfuse 0154005 | 2 | Input protection |
| TVS Diodes | 350V | SMAJ350A | 2 | Overvoltage protection |

### 3. Gate Drive Circuit

Each MOSFET needs:
- Isolated gate driver IC (ADuM4225, 1.5kV isolation)
- Bootstrap capacitor (~100nF, 500V)
- Bootstrap diode (UF4007 or fast recovery)
- Gate resistor (10Ω-100Ω, high voltage rated)
- Pull-down resistor (10kΩ)
- Optional: Gate turn-off snubber network

### 4. Control Signals (from RP2354A)

The RP2354A should generate:
- **PWM_H1** - High-side upper MOSFET (Q1)
- **PWM_L1** - Low-side lower MOSFET (Q2)  
- **PWM_H2** - High-side upper MOSFET (Q3)
- **PWM_L2** - Low-side lower MOSFET (Q4)
- **DEAD_TIME** - Ensure no shoot-through
- **OSHOCK** - Overcurrent shutdown
- **TEMP** - Temperature monitoring input

### 5. Protection Circuits

**Overcurrent Protection:**
- Current sense resistor (0.01Ω - 0.1Ω, 5W) in DC link
- Op-amp or comparator to detect overcurrent
- Input to RP2354A for shutdown

**Overvoltage Protection:**
- TVS diode across DC bus
- Zener or comparator circuit to detect overvoltage

**Temperature Protection:**
- NTC thermistor near MOSFETs
- Connect to RP2354A ADC for thermal monitoring

**Bootstrap Circuit (per half-bridge):**
```
VCC Drive ──[10kΩ]──┬── Bootstrap Cap ── GND
                    │
                   IC
```

## Integration Points

### Connect to Boost Section:
- **VOUT-BOOST** → Inverter DC Bus (+)
- **GND** → Inverter DC Bus (-)

### Connect to Control Section:
- **INVERT_PWM1** → Gate driver 1 input
- **INVERT_PWM2** → Gate driver 2 input  
- **INVERT_PWM3** → Gate driver 3 input
- **INVERT_PWM4** → Gate driver 4 input
- **INVERT_CS** → Current sense input
- **INVERT_TEMP** → Temperature sensor input
- **INVERT_FAULT** → Fault output to MCU

### Connect to Charging Section:
- Ensure inverter doesn't back-feed into charger during charging

## Recommended Schematic Structure

Add to **Lamoka1.kicad_sch** Root sheet:
```
Sheet: Inverter Section
Size: A3 (420 x 297 mm)
Sheetfile: Inverter.kicad_sch
Pins:
  VIN    - DC bus input from boost (high voltage)
  GND    - Common ground
  PWM1   - PWM from MC to gate driver 1
  PWM2   - PWM from MC to gate driver 2
  PWM3   - PWM from MC to gate driver 3
  PWM4   - PWM from MC to gate driver 4
  CS     - Current sense output to MC
  TEMP   - Temperature input to MC
  FAULT  - Fault input from MC
  AC_OUT - Filtered AC output (connect to terminal block)
```

## Component Sourcing Suggestions

### MOSFET Options (200V, fast switching):
- **IRF740** - 400V, 15A, 150ns
- **IXFN110N20P3** - 200V, 110A, very low Rds(on)
- **STP55NF06L** - 60V but excellent performance

### Gate Driver Options:
- **ADuM4225** - 2.5kV, 4A peak, isolated
- **HCPL-3120** - 6kV, 1A peak
- **PXV2110** - 1500V, 3A peak

### Inductors:
- **Coilcraft LPS4018-4720** - 4.7µH, 20A
- **Würth 744244700** - 4.7µH, 4A
- **Murata 14110111A** - 4.7µH, 2.5A

### Capacitors:
- **DC Link**: Panasonic EEU-FC1V471 (470µF, 35V) - use 450V version
- **Film Caps**: WIMA FKPX (10µF, 630V)

## Safety Notes

⚠️ **HIGH VOLTAGE CIRCUIT - HANDLE WITH CARE**
⚠️ **Minimum 10mm creepage required**
⚠️ **Proper isolation between high voltage and low voltage**
⚠️ **Use double-insulated wire for high voltage connections**
⚠️ **Consider adding a "Safety Enable" switch in series with VIN**

## Testing Sequence

1. **Bench Test (Low Voltage):**
   - Use 12V or 24V supply for initial testing
   - Verify deadtime and switching behavior
   - Measure switching losses

2. **DC Bus Test:**
   - Apply boosted voltage slowly
   - Monitor for ringing/overshoot
   - Verify gate drive isolation

3. **Load Test:**
   - Start with resistive load (light bulb)
   - Monitor temperature rise
   - Verify output waveform quality

4. **Full Integration:**
   - Test with solar input simulation
   - Verify bidirectional operation
   - Test fault conditions