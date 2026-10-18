## What is the Portal Interface Engine?

PIE is the intended operating system for Lamoka models. It provides simple "portals" to whatever you would like with your unit.


## Compatibility

PIE **V1.9** is the first revision that runs on the **Lamoka1/2 main board**. It scales
the UI to the main board's five WS2812B LEDs, reads the TMP235 sensors on GPIO 26/27,
uses the RP2350 hardware TRNG for entropy, and drives the H-bridge power stage
(see `PIE-v1.9-information.md`).

PIE **V1.8** runs on the **Lamoka1-DEV-Brain** (ten LEDs, analog entropy). It does
**not** run on the main Lamoka1/2 board unmodified:

| V1.8 uses | Main board |
| --- | --- |
| GPIO 0 — button | button (matches) |
| GPIO 11 — LED data | LED data (matches) |
| GPIO 5 — digital entropy input | not connected |
| GPIO 26 — ADC entropy input | `TEMP_CTRL`, a TMP235 temperature sensor output |
| GPIO 27 — ADC entropy input | `TEMP_INV`, a TMP235 temperature sensor output |
| GPIO 28, GPIO 29 — ADC entropy inputs | not connected |
| `NUM_LEDS` = 10 | 5 WS2812B LEDs fitted (D1–D5) |

Reading GPIO26 and GPIO27 as entropy would instead read the two temperature sensors, and
driving ten LEDs would address five that are not fitted. The menu, the brightness and
speed bars, the binary score display, and the Fireball game all index across the full
array, so the interface would be truncated rather than merely dimmer.

Earlier PIE revisions (`V1.ino` through `V1.7.ino`) were also written for the DEV-Brain
and are kept as history. V1.8 remains the reference for the DEV-Brain.
