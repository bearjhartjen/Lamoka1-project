> [!Warning]
> This page describes the **intended** boot sequence for the Lamoka1/2. Parts of it are now
> implemented by **PIE V1.9** (input-rail and 17 V bus validation, soft-start of the
> H-bridge, closed-loop regulation, fault latching); other parts (phase-4 output
> measurement, the fan has no PWM/tach, etc.) remain design targets. PIE V1.8 does not
> run on the main board — see [`README.md`](README.md).

When the Lamoka1/2 is connected to USB-C 5v, the intent is for it to present a
version of the PIE interface adapted to the main board's five LEDs. As soon as
the unit's MUX (ADS7128IRTER made by TI) shows a voltage on the input net that
is above 11v yet below 14v, the device will commence a boot-up sequence. The
unit's fan (FAD1-04010BHLW11 made by Qualtek) is only connected to the buck 5v
rail (TPS62160DSGR made by TI). The boot sequence will go upstream to verify
everything is working correctly.

We like to think of this as the unit waking up and checking its own pulse
before it is allowed to do anything dangerous. Nothing gets to the H-bridge
until the low-voltage side proves it is alive.
 ## First phase 

 The MUX (ADS7128IRTER) will be detecting the source rail. If this rail is at an appropriate voltage (which it will be in order to even start the sequence), stage one will be validated as functioning. 

 ## Second phase

 Next, the MCU (RP2354A made by Raspberry Pi) verifies the buck 5V rail is present, which also powers the fan (FAD1-04010BHLW11). GPIO5 and GPIO6 are unused, so there is no PWM speed command or tach verification; the fan runs whenever the 5V rail is up. The two TMP235A2DBZR temperature sensors are monitored separately from fan control. Once the 5V rail is confirmed, the second phase will be confirmed functioning. 

 ## Third phase

 Up next, the MUX (ADS7128IRTER) will detect the output of the 11-14v to 17v boost converter (LM5122MHX/NOPB made by TI). If this rail is within 3/4 of a volt of 17v, this stage will be verified as correctly operating. 

 ## Fourth phase

 Things will get a bit tricky now; we are monitoring alternating current waveforms after this point. 