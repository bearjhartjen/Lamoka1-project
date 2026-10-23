## What is the Portal Interface Engine?

PIE is the operating system for Lamoka models. It gives you simple "portals" to
whatever you want to do with your unit — lights, games, settings, sleep, USB
bootloader entry — through one button and a strip of addressable LEDs. Tap to
move, double-tap to go back, hold to select.

## Compatibility

PIE **V1.9** is the first release that runs on the **Lamoka1/2**. It scales
the UI to the main board's five WS2812B LEDs, reads the TMP235 sensors on GPIO 26/27,
uses the RP2350 hardware TRNG for entropy, and drives the H-bridge power stage
(see `PIE-v1.9-information.md`). The V1.9 output target is **120.0 V RMS at
the outlet**, on a **21.75 V bus**. The board has no isolated output sense, so
the firmware regulates a droop-compensated *estimate* of the outlet —
`V_out = A_DROOP × V_drive_rms − B_DROOP × I_sec_est` — using the
transformer's catalogue constants, which hold the outlet within about 3 % of
120.0 V RMS for the droop-model term alone. The sweep, the 3.3 V rail and the
dividers add a few percent, so treat the as-built unit as about ±8 % until it
is calibrated against a meter. No unit needs to be calibrated or fitted to run.
The power stage has never been energised.

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

PIE is kept as two heads: `V1.8.ino` for the DEV-Brain and `V1.9.ino` for
the Lamoka1/2. Both are maintained in place, and git history holds the earlier
`V1.0`–`V1.7` releases. V1.8 remains the reference for the DEV-Brain.
