# How the Lamoka1/2 boots

This is the boot sequence for the Lamoka1/2, stage by stage.

> [!NOTE]
> PIE V1.9 implements the input-rail and 21.75 V bus validation, the soft start
> of the H-bridge, the closed-loop regulation and the fault latching. The
> later stages are the design the board was laid out for, and they come with
> later Lamoka1 models. PIE V1.8 does not run on the main board — see
> [`README.md`](README.md).

When the Lamoka1/2 is connected to USB-C 5 V, it presents the PIE interface
adapted to the main board's five LEDs. As soon as the unit's mux (the
ADS7128IRTER, made by TI) shows a voltage on the input net that is above 11 V
yet below 14 V, the device starts its boot-up sequence. The unit's fan
(FAD1-04010BHLW11, made by Qualtek) is only connected to the buck 5 V rail
(TPS62160DSGR, made by TI).

We like to think of this as the unit waking up and checking its own pulse
before it is allowed to do anything dangerous. Nothing gets to the H-bridge
until the low-voltage side proves it is alive.

## First phase

The mux (ADS7128IRTER) detects the source rail. If this rail is at an
appropriate voltage — which it has to be in order to even start the sequence —
stage one is validated as functioning.

## Second phase

Next, the MCU (RP2354A, made by Raspberry Pi) verifies that the buck 5 V rail
is present, which also powers the fan (FAD1-04010BHLW11). GPIO5 and GPIO6 are
unused, so there is no PWM speed command and no tach verification; the fan
runs whenever the 5 V rail is up. The two TMP235A2DBZR temperature sensors are
monitored separately from fan control. Once the 5 V rail is confirmed, the
second phase is confirmed functioning.

## Third phase

Up next, the mux (ADS7128IRTER) detects the output of the 11–14 V to 21.75 V
boost converter (LM5122MHX/NOPB, made by TI). If this rail is within 0.9 V of
21.75 V, this stage is verified as correctly operating. Once the bridge is
running, the window widens to ±1.5 V.

## Fourth phase

Things get a bit tricky now: we are monitoring AC waveforms past this point.
The bridge is armed by the user, soft-started, and then run in closed loop on
the estimated outlet voltage.

> [!CAUTION]
> Nothing in this sequence measures the 120 V RMS side of the transformer.
> The bridge runs on an estimate of the outlet, and the only way to know the
> real outlet voltage is to measure it with a meter. See
> [`../HARDWARE/BRINGUP.md`](../HARDWARE/BRINGUP.md).
