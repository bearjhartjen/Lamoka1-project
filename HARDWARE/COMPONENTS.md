## Parts on the Lamoka1/2, in plain English

(component alternatives may be used — this is a reference, not a shopping list)

> [!Warning]
> This is not verified to be the full BOM for the Lamoka1/2. For all components, please refer to the official BOM. This is purely for reference and learning, not to be used as a BOM.

> Reference designators in parentheses match the schematic. Where the schematic carries no manufacturer field, the manufacturer given here is taken from the part number.

We put this list together so anyone can read the schematic without a datasheet
in hand. It is not the BOM, and it is not the order list — the KiCad BOM and
`HARDWARE/Lamoka1-half fab files/Lamoka1-half BOM.csv` are. But if you want to
know what the CSD19536KCS is, or why the boost inductor is a Coilcraft
XAL1580, this is the place.
 

NMOS for H-bridge (Inversion-section): CSD19536KCS made by TI  x4 (Q3, Q4, Q5, Q6)

 Transformer (Inversion-section): VPT24-6670 made by Triad Magnetics, 160 VA, 8 % regulation. Mounted off-board and wired to the PCB at J1; `HARDWARE/Transformer-wiring.md` records which lead goes on which terminal. Its 12 V winding is the one the H-bridge drives and the 115 V winding feeds the outlet, so the lead colours must not be matched to the J1 net names. The eight-lead colour map is the same as the 100 VA VPT24-4170 it replaces, so the J1 harness map does not change; the harness current capability does, and the part is 1.6 kg instead of 1.0 kg.

 Main microcontroller for full system: RP2354A made by Raspberry Pi (U1)

 Crystal for the microcontroller: ABM8-272-T3 made by Abracon (X1)

 Inductor for the microcontroller's internal core regulator (VREG_LX to 1V1): AOTA-B201610S3R3-101-T made by AOTA, 3.3 uH (L1)

 Addressable RGB LEDs: WS2812B made by WorldSemi x5 (D1-D5)

 Level shifter on the LED data line (3v3 to 5v): SN74AHCT1G125DBVR made by TI (U3)

 High-side NMOS for boost converter (Inversion-section): CSD17304Q3 made by TI (Q1). The same 30 V part as Q2, on the same DQG0008A footprint; the 25 V CSD16323Q3 it replaces left only about 3 V of headroom on the 21.75 V bus's switching node

 Low-side NMOS for boost converter (Inversion-section): CSD17304Q3 made by TI (Q2)

 Inductor for boost converter (Inversion-section): XAL1580-182MEB made by Coilcraft, 1.8 uH (L2)

 Current-sense resistor for boost converter (between CSP and CSN): CSNL2512FT2L00 made by Stackpole, 2 milliohm, 1 %, 2 W, 2512 (Rsense1). The value stays at 2 milliohm: an earlier finding proposed 5 milliohm, which is wrong for a 100 W single-phase load — the 120 Hz bus-current pulsation gives a roughly 27 A inductor crest, and a 5 milliohm shunt would clip the output to about 57 W. The package moved from 1206 to 2512 for thermal headroom only (38 % of the derated rating instead of 68 % at a 120 °C ambient). It is a two-terminal part, so no Kelvin sensing is implemented

 Top resistor for the boost converter feedback divider: CRCW040264K9FKED made by Vishay, 64.9 kOhm, 1 %, 0402 (Rfbt1). Sets the 21.75 V bus: 1.2 x (1 + 64.9/3.79) = 21.749 V

 Bottom resistor for the boost converter feedback divider: CRCW04023K79FKED made by Vishay, 3.79 kOhm, 1 %, 0402 (Rfbb1)

 Slope compensation resistor for the boost converter: RC0201FR-0749K9L made by Yageo, 49.9 kOhm, 1 %, 0201 (Rslope1). The 105 kOhm it replaces gave a slope-compensation factor of 0.525 where the datasheet asks for K >= 1

 Compensation resistor for the boost converter: RC0201FR-0780K6L made by Yageo, 80.6 kOhm, 1 %, 0201 (Rcomp1)

 Compensation capacitor for the boost converter: CGA4F1C0G2A153J085AE made by TDK, 15 nF, C0G, 0805 (Ccomp1)

 Compensation capacitor for the boost converter: GRM1555C1H448JA01D made by Murata, 44.8 pF, C0G, 0402 (Ccomp2). With the values above the boost loop crosses over at about 9.3-11.8 kHz with roughly 80 degrees of phase margin

 Output capacitors for the boost converter bus: 35SVPK180M made by Panasonic Industry, 180 uF, 35 V, 8 x 11.9 mm, x3 (Cout1, Cout2, Cout3). The 25 V parts they replace ran at 89.5 % of their rating on the 21.75 V bus

 Output capacitor for the boost converter bus: C3216X5R1V226M160AC made by TDK, 22 uF, 35 V, X5R, 1206 (Coutx1). A 47 uF 1206 at 35 V does not exist

 Resistors for the voltage-sensing dividers on the boost bus and the two bridge midpoints: RC0603FR-0768KL made by Yageo, 68 kOhm, 1 %, 0603, x3 (R27, R29, R31). The 43.2 kOhm and 45.3 kOhm they replace clipped at 17.56 V and 18.25 V, below the 21.75 V bus, so the firmware was reading a saturated ADC

 Bypass capacitors for the gate drivers' supply pins: GRM21BZ71E106KE15L made by Murata, 10 uF, 25 V, X7R, 0805, x2 (C34, C36) and CL10B104KB8NNNC made by Samsung Electro-Mechanics, 100 nF, 0603, x2 (C35, C37). One pair at each of U5 and U6's VDD pin (pin 7); the UCC27735 datasheet wants CVDD >= 10 x CBOOT and the bootstrap capacitors are 1 uF. The nearest bulk capacitor used to be 49-84 mm away

 Bootstrap diode for boost converter: SK215ATR (D6)

 Gate driver for H-bridge NMOSs: UCC27735DR made by TI x2 (U5, U6)

 IC for 11-14v to 21.75v boost converter: LM5122MHX/NOPB made by TI (U4)

 IC for 11-14v to 5v buck converter: TPS62160DSGR made by TI (U7)

 Input capacitor for the 11-14v rail, shared by the boost and the buck converters: TMK212BBJ106KG-T made by Taiyo Yuden, 10 uF (C24)

 Cooling fan for unit: FAD1-04010BHLW11 made by Qualtek

 Fan connection: two solder wire pads on the PCB (J4 = FAN GND, J6 = FAN 5V); no header is fitted, the fan leads solder straight to the board

 Output capacitor for buck converter: GRM31CR71A226KE15L made by Murata (C25)

 Output capacitor for the 3v3 regulator: T491A226K010AT made by KEMET, 22 uF, 10 V, 3216 A case, tantalum (C13). The AMS1117 wants 22 uF *with* ESR — a near-zero-ESR X5R MLCC is the instability condition, so this is a stability fix and not a nicety

 Inductor for buck converter: PA4332.222NLT made by Pulse Electronics (L3)

 Bottom resistor for buck converter FB: RC0603FR-07180KL made by Yageo (R19)

 Top resistor for buck converter FB: CRCW0402953KFKED made by Vishay Dale (R20)

 Power-good pull-up resistor for buck converter: CRCW0402100KFKED made by Vishay Dale (R21)

 Diode for 5v source to eliminate buck output and USB 5v interactions: SS14-E3/61T made by Vishay x2 (CR1, CR2)

Bootstrap diode for the H-bridge gate drivers: SS14-E3/61T made by Vishay x2 (D11, D12)

 Temp sensor for unit: TMP235A2DBZR made by TI x2 (U8, U9)

 Current-sense shunt for source-rail monitoring (+BATT to CSP): CSNL2512FT2L00 made by Stackpole, 2 milliohm, 1 %, 2 W, 2512 (RCS1)

 Current-sense amplifier for source-rail monitoring: INA240A2DR made by TI (IC1)

 Connector for transformer's connection to the PCB: 2606-1108/010-000 made by Wago (J1)

 Connector for 11-14v DC source: 2606-1352 made by Wago (J2)

 Outlet for inverter (120v AC North American): 3-119-054 made by SCHURTER Inc.

 Connector for hooking outlet to PCB: 2604-1103 made by Wago (J5)

 Multiplexer for voltage and current monitoring: ADS7128IRTER made by TI (U10)

 Tactile button for RP2354A control: B3F-4055 made by Aratas (Formerly Omron Components) (SW1)

 Converter to go from 5v to 3v3 for control: AMS1117-3.3 made by Advanced Monolithic Systems (U2)

 USB-C receptacle for 5v input: TYPE-C-31-M-12 made by HRO (J3)

