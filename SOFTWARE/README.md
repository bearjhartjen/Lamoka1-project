## What is the Portal Interface Engine?

PIE is the intended operating system for Lamoka models. It provides simple "portals" to whatever you would like with your unit.


## Compatibility

PIE V1.8 runs on the **Lamoka1-DEV-Brain**. It is the version developed and burn-tested on that board, and the ten-LED, one-button interface it implements matches it.

> [!Warning]
> **PIE V1.8 does not run on the main Lamoka1/2 board.** The main board is not the DEV-Brain, and the differences are not cosmetic.

| V1.8 uses | Main board |
| --- | --- |
| GPIO 0 — button | button (matches) |
| GPIO 11 — LED data | LED data (matches) |
| GPIO 5 — digital entropy input | not connected |
| GPIO 26 — ADC entropy input | `TEMP_CTRL`, a TMP235 temperature sensor output |
| GPIO 27 — ADC entropy input | `TEMP_INV`, a TMP235 temperature sensor output |
| GPIO 28, GPIO 29 — ADC entropy inputs | not connected |
| `NUM_LEDS` = 10 | 5 WS2812B LEDs fitted (D1–D5) |

Reading GPIO26 and GPIO27 as entropy would instead read the two temperature sensors, and driving ten LEDs would address five that are not fitted. The menu, the brightness and speed bars, the binary score display, and the Fireball game all index across the full array, so the interface would be truncated rather than merely dimmer.

No firmware currently drives the main board's power stage. [`Operation.md`](Operation.md) describes the intended boot sequence, which is a design target and not working behaviour.

Earlier PIE revisions (`V1.ino` through `V1.7.ino`) were also written for the DEV-Brain and are kept as history.
