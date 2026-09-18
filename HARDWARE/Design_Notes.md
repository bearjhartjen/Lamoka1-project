## MCU GPIO pin connections

 GPIO0: Connected high to 3v3 by a 10k resistor, then goes to a button that when pressed pulls the pin low to ground 

 GPIO1: Controls Q3 Mosfet through UCC27735DR IC

 GPIO2: Controls Q5 Mosfet through UCC27735DR IC

 GPIO3: Controls Q4 Mosfet through UCC27735DR IC

 GPIO4: Controls Q6 Mosfet through UCC27735DR IC

 GPIO5: RPM speed signal from NF-A4x20 5V PWM (GPIO5 is pulled up via a 10k resistor to 3v3 rail)

 GPIO6: PWM signal to NF-A4x20 5V PWM

 GPIO11: Goes to a level shifter to get up to 5v then to a 330 ohm resistor then connects to string of ten WS2812B lights arranged next to each other from left to right with D1 starting and D10 finishing, the press button is to the left of D1

 GPIO26: Output of TMP235A2DBZR temp sensor located in the Control-Section

 GPIO27: Output of TMP235A2DBZR temp sensor located in the Inverter-Section 

---

## H-bridge (Inversion-Section)

| H-bridge leg | High-side MOSFET | Low-side MOSFET | Gate driver | RP2354A control |
|---|---|---|---|---|
| Leg A | Q3 | Q5 | U5 | GPIO1 / GPIO2 |
| Leg B | Q4 | Q6 | U6 | GPIO3 / GPIO4 |

> **H-bridge switching:** Q3/Q5 and Q4/Q6 are the complementary MOSFET pairs for Leg A and Leg B respectively. Dead time must be implemented between the high-side and low-side devices of each leg to prevent shoot-through.