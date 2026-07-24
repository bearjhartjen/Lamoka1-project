# Secret-DEV-Brain — PCBWAY ACCURATE REV11

This model represents the assembled Lamoka1-DEV-Brain that PCBWay
is actually delivering. It is a refinement of the original
realistic multicolor model, not a visual redesign. The original
USB-C, button, RP2354A and other component bodies, exact KiCad
flamingo/cactus logo, wordmark, and recessed red/blue NeoPixels
remain intact.

## Preferred file

Open `Secret-DEV-Brain_PCBWAY_ACCURATE_REV11.3mf` in Bambu Studio.
It is an editable four-part model and contains no pre-sliced G-code.

The package also includes an editable three-up 3MF, OBJ/MTL,
four aligned fallback STLs, previews, validation, checksums, and
reproducible source.

## AMS order

1. A1 — Blue
2. A2 — Red
3. A3 — Black
4. A4 — Silver

## PCBWay-delivered J1

J1 is DNP. PCBWay installs no male header, female header, or other
connector. The model contains exactly the thirteen bare plated
through-holes that remain on the manufactured PCB.

- 2.54 mm pitch.
- 1.70 mm copper-pad diameter/width.
- 1.00 mm finished-model bore matching the production drill.
- Pin 1 has the KiCad square pad; pins 2–13 are round.
- The bores pass through the full PCB thickness.
- J1 is absent from the PCBWay BOM and pick-and-place files.
- The silver top rings use the real KiCad solder-mask openings:
  2.20 mm square for pin 1 and 2.00 mm round for pins 2–13.
- The rings stand 0.16 mm above the black surface—two layers at the
  recommended profile—so the slicer cannot hide the silver.

## Physical-print cleanup

The previous print exposed 0.04 mm silver sheets and detached
sub-nozzle slivers that are smaller than one selected print layer.
Those non-readable artifacts were removed. Actual component bodies,
packages, logo, text, USB-C shell, button, and LED housings were not
restyled or enlarged.

## P1S settings

- 0.4 mm nozzle.
- 0.08 mm layer height.
- Arachne wall generator.
- Slow small perimeters to about 15–20 mm/s for clean J1 rings.
- Prime tower on; use calibrated black-to-silver flushing.
- Support only beneath the USB-C shell as with the successful print.
- No support is needed at J1 because it is a bare hole row.
