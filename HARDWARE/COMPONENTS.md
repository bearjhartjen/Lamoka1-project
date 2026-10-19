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

 Transformer (Inversion-section): VPT24-4170 made by Triad Magnetics, 100 VA. Mounted off-board and wired to the PCB at J1; `HARDWARE/Transformer-wiring.md` records which lead goes on which terminal. Its 12 V winding is the one the H-bridge drives and the 115 V winding feeds the outlet, so the lead colours must not be matched to the J1 net names.

 Main microcontroller for full system: RP2354A made by Raspberry Pi (U1)

 Crystal for the microcontroller: ABM8-272-T3 made by Abracon (X1)

 Inductor for the microcontroller's internal core regulator (VREG_LX to 1V1): AOTA-B201610S3R3-101-T made by AOTA, 3.3 uH (L1)

 Addressable RGB LEDs: WS2812B made by WorldSemi x5 (D1-D5)

 Level shifter on the LED data line (3v3 to 5v): SN74AHCT1G125DBVR made by TI (U3)

 Low-side NMOS for boost converter (Inversion-section): CSD17304Q3 made by TI (Q2)

 High-side NMOS for boost converter (Inversion-section): CSD16323Q3 made by TI (Q1)

 Inductor for boost converter (Inversion-section): XAL1580-182MEB made by Coilcraft, 1.8 uH (L2)

 Current-sense resistor for boost converter (between CSP and CSN): CSNL1206FT2L00 made by Bourns, 2 milliohm (Rsense1)

 Bootstrap diode for boost converter: SK215ATR (D6)

 Gate driver for H-bridge NMOSs: UCC27735DR made by TI x2 (U5, U6)

 IC for 11-14v to 17v boost converter: LM5122MHX/NOPB made by TI (U4)

 IC for 11-14v to 5v buck converter: TPS62160DSGR made by TI (U7)

 Input capacitor for the 11-14v rail, shared by the boost and the buck converters: TMK212BBJ106KG-T made by Taiyo Yuden, 10 uF (C24)

 Cooling fan for unit: FAD1-04010BHLW11 made by Qualtek

 Fan connection: two solder wire pads on the PCB (J4 = FAN GND, J6 = FAN 5V); no header is fitted, the fan leads solder straight to the board

 Output capacitor for buck converter: GRM31CR71A226KE15L made by Murata (C25)

 Inductor for buck converter: PA4332.222NLT made by Pulse Electronics (L3)

 Bottom resistor for buck converter FB: RC0603FR-07180KL made by Yageo (R19)

 Top resistor for buck converter FB: CRCW0402953KFKED made by Vishay Dale (R20)

 Power-good pull-up resistor for buck converter: CRCW0402100KFKED made by Vishay Dale (R21)

 Diode for 5v source to eliminate buck output and USB 5v interactions: SS14-E3/61T made by Vishay x2 (CR1, CR2)

Bootstrap diode for the H-bridge gate drivers: SS14-E3/61T made by Vishay x2 (D11, D12)

 Temp sensor for unit: TMP235A2DBZR made by TI x2 (U8, U9)

 Current-sense shunt for source-rail monitoring (+BATT to CSP): CSNL2512FT2L00 made by Bourns, 2 milliohm (RCS1)

 Current-sense amplifier for source-rail monitoring: INA240A2DR made by TI (IC1)

 Connector for transformer's connection to the PCB: 2606-1108/010-000 made by Wago (J1)

 Connector for 11-14v DC source: 2606-1352 made by Wago (J2)

 Outlet for inverter (120v AC North American): 3-119-054 made by SCHURTER Inc.

 Connector for hooking outlet to PCB: 2604-1103 made by Wago (J5)

 Multiplexer for voltage and current monitoring: ADS7128IRTER made by TI (U10)

 Tactile button for RP2354A control: B3F-4055 made by Aratas (Formerly Omron Components) (SW1)

 Converter to go from 5v to 3v3 for control: AMS1117-3.3 made by Advanced Monolithic Systems (U2)

 USB-C receptacle for 5v input: TYPE-C-31-M-12 made by HRO (J3)

