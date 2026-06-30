## Top priority
 Put here the photos of Tomachie grading for boost converter, and a photo of boost converter schem 



<div align="center">


![Status](https://img.shields.io/badge/status-early%20design-yellow.svg)
![Hardware License](https://img.shields.io/badge/Hardware-CERN--OHL--W%20v2.0-orange.svg)
![Firmware License](https://img.shields.io/badge/Firmware-GPLv3-blue.svg)
</div>

---

## Scope of Lamoka1

 The **Lamoka1** is a fully open-source, pure sine wave inverter and charger system, controlled by an RP2350. It's built to be fully transparent, modular, and repeatable, aiming to be the best in its class rather than a closed, disposable black box. Every schematic, every line of firmware, and every design decision is published — nothing is hidden behind proprietary lock-in.

It's designed for solar enthusiasts, electrical engineers, and everyday users alike, with a simple physical interface hiding a fully configurable system underneath.


---

## Notable features

-  **Pure sine wave output** — or virtually any waveform you want, up to a 180V peak limit
-  **Modular & repairable** — built from replaceable, well-documented sections instead of a sealed, disposable unit
-  **Fully configurable** — every behavior can be customized through user code, not locked behind firmware
-  **Dual operating modes** — standard sine-wave operation, or a stripped-down "dumb mode" for purely resistive loads (see below)
-  **Adaptive cooling** — per-section temperature sensing drives a PWM-controlled fan, scaling speed precisely instead of crude on/off switching
-  **Minimal UI** — ten RGB LEDs and a single push button handle the entire physical interface

---

## Specifications

| Parameter | Value |
|:---|:---|
| **DC Input** | 5–40V |
| **Inverter Output** | AC 120 Vrms or Configurable |
| **Control** | RP2350 microcontroller |
| **User Interface** | 10× RGB LEDs + 1 push button |
| **Cooling** | Pulse width modulated fan |
---


## Software-Defined Control (INVERTER)

The entire H-bridge is driven directly by the RP2350 — there's no separate analog waveform generator or fixed-function inverter chip in the way. Every waveform is articulated by the microcontroller.

Out of the box, Lamoka1 comes with **basic stock firmware**: clean sine wave output and a resistive-load "dumb mode". This can be a starting point, not a limit of function.

> [!TIP]
> Because the RP2350 controls switching directly, the unit can be reprogrammed to do far more than the stock firmware — custom waveforms, different control strategies, alternative protection logic, whatever you can get the math to do safely. This is where the open-source firmware license really matters: if you build something, you can share it back with the community, and anyone else with a Lamoka1 can run it.

---


## License

Lamoka1 uses a split license, matching the folder you're in:

- **Hardware**  — Licensed under [CERN-OHL-W v2.0](https://ohwr.org/cern_ohl_w_v2.txt)
- **Firmware**  — Licensed under [GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html)
---

## Lamoka project goals

We want anyone to be able to pick up any Lamoka unit, and use it for what they need, whether that be charging up a 12v battery with AC power, or managing whole solar setups. These units are designed to be reliable and usefull. We are taking the "smart" functions of inverter/chargers and actually allowing the unit to utilize its smarts, there is no reason that a smart inverter charger NEEDs a bluetooth or WIFI connection, the unit should just take care of everything. 


## Lamoka1 caution 

The Lamoka1 is simply designed to be as cheap as possible while still being the MVP product, this is just so we can get people interested while maintaining funds to keep going. 

