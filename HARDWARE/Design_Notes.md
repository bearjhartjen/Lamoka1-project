# Design notes — the Lamoka1/2

These notes are the design intent behind the Lamoka1/2: why each pin goes where
it goes, what the constraints are, and what we deliberately left out. The
schematic and the board are the specification; this file explains them. We
wrote it so a future version of ourselves — or anyone picking the project up
— can follow the reasoning instead of rediscovering it.

## MCU GPIO pin connections
 GPIO0: Connected high to 3v3 by a 10k resistor, then goes to a B3F-4055 button made by Aratas that, when pressed, pulls the pin low to ground 

 GPIO1: Controls Q3 MOSFET (CSD19536KCS made by TI) through UCC27735DR IC made by TI

 GPIO2: Controls Q5 MOSFET (CSD19536KCS made by TI) through UCC27735DR IC made by TI

 GPIO3: Controls Q4 MOSFET (CSD19536KCS made by TI) through UCC27735DR IC made by TI

 GPIO4: Controls Q6 MOSFET (CSD19536KCS made by TI) through UCC27735DR IC made by TI

 GPIO5: Not in use (unused, no connection)

 GPIO6: Not in use (unused, no connection)

 GPIO8: Connects to ADS7128IRTER made by TI SDA pin for serial data pulled up to 3v3 by a 4.7k resistor

 GPIO9: Connects to ADS7128IRTER made by TI SCL pin for serial clock pulled up to 3v3 by a 4.7k resistor

 GPIO11: Goes to a level shifter to get up to 5v, then to a 330 ohm resistor, then connects to a string of five WS2812B lights made by WorldSemi arranged next to each other from left to right with D1 starting and D5 finishing. The press button (B3F-4055 made by Aratas) is to the left of D1.

 GPIO26: Output of the TMP235A2DBZR temp sensor made by TI located in the Control-Section

 GPIO27: Output of the TMP235A2DBZR temp sensor made by TI located in the Inverter-Section 

---


## MUX AIN pin connections (ADS7128IRTER made by TI)

 AIN0: input rail voltage sensing, top resistor 36.5k, bottom resistor 10k, 47nF cap to ground on AIN pin  

 AIN1: output of 17v boost converter voltage sensing, top resistor 43.2k, bottom resistor 10k, 47nF cap to ground on AIN pin 

 AIN2: SW_A (Leg A of H-bridge) voltage sensing, top resistor 45.3k, bottom resistor 10k, 47nF cap to ground on AIN pin

 AIN3: SW_B (Leg B of H-bridge) voltage sensing, top resistor 45.3k, bottom resistor 10k, 47nF cap to ground on AIN pin 

 AIN4: Current sensing for source rail, 11-14v goes through a 2m shunt; the shunt is connected to the INA240A2DR IC, the OUT pin of that IC goes to a 22 ohm resistor to the AIN pin, the AIN pin has a 100nF cap to ground.


---

## H-bridge (Inversion-Section) — four CSD19536KCS NMOSs made by TI, two UCC27735DR gate drivers made by TI

| H-bridge leg | High-side MOSFET | Low-side MOSFET | Gate driver | RP2354A control |
|---|---|---|---|---|
| Leg A | Q3 | Q5 | U5 | GPIO1 / GPIO2 |
| Leg B | Q4 | Q6 | U6 | GPIO3 / GPIO4 |

> **H-bridge switching:** Q3/Q5 and Q4/Q6 are the complementary MOSFET pairs for Leg A and Leg B respectively. Dead time must be implemented between the high-side and low-side devices of each leg to prevent shoot-through.

---

## Design constraints

 We are refraining from putting in a 5v USB output for the Lamoka1/2. This is because our 11-14v to 5v buck converter (TPS62160DSGR made by TI) is designed for usage up to one amp. In future Lamoka models, if a USB output (preferably USB-C) is established, the buck converter would have to be beefed up; however, it would be even better if we had a USB-C PD circuit in so the user could get a better experience across a wider range of usage. 

 The transformer applied to the Lamoka1/2 is the VPT24-4170 made by Triad Magnetics, a 100 VA part, about 2.2 lb (1 kg) and around $80. It mounts off-board and wires to the PCB at J1. We understand this may be a strong downside to some people; however, we found this was the best widely available option, plus it adds efficiency prospects. The board drives its 12 V winding and takes the outlet from the 115 V winding; `HARDWARE/Transformer-wiring.md` is the lead-to-terminal spec, and the lead colours must not be matched to the J1 net names.


## User interface

The user interface is on the main board rather than on a board of its own. `User-Interface-Section.kicad_sch` is a hierarchical sheet inside the same project as the control and inversion sections, and the LEDs sit on the main 95 x 95 mm outline. The main board populates **five** WS2812B LEDs (D1-D5) and one button.

An earlier plan split the interface onto a separate board joined to the main board with mouse bites and a wired connector, so that the large VPT24-4170 transformer and the airflow it needs could be accommodated separately. The current layout does not use that split: there are no mouse-bite features and no inter-board connector.

The Lamoka1-DEV-Brain is a standalone development board, not a half of the main board. It carries **ten** WS2812B LEDs (D1-D10), which is the count PIE V1.8 is written for.

