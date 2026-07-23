# Secret-DEV-Brain — four-color AMS print

Use `Secret-DEV-Brain_Multicolor.3mf` in Bambu Studio. It is an editable
single assembly with four named parts and contains no pre-sliced G-code.

## AMS mapping

1. A1 — Blue
2. A2 — Red
3. A3 — Black
4. A4 — Silver

The NeoPixels alternate D1 Red, D2 Blue, D3 Red, and so on through D10 Blue.
The PCB and dark packages are Black. Metal details, the exact repository
flamingo/cactus logo, the `Secret-DEV-Brain` label, and the NeoPixel
housings are Silver.

## Recommended Bambu Studio setup

- Printer/nozzle: Bambu Lab P1S, 0.4 mm nozzle.
- Layer height: 0.08 mm is recommended for the cleanest logo and lettering.
- Wall generator: Arachne.
- Supports: off.
- Place the model flat on its PCB back, then confirm it is dropped to the bed.
- Confirm the four named parts map to the AMS slots above before slicing.
- Use compatible PLA profiles for all four spools.

The 3MF is the preferred file. If it does not import correctly, select all
four files in `parts/` at the same time and choose **load as a single object
with multiple parts**. Do not center the STLs separately; they already share
the same coordinates. The OBJ and MTL are a second aligned fallback.

## Accuracy changes

- NeoPixel housings come from the repository's original 1:1 STL and match its
  WS2812B 5.0 x 5.0 mm package. The separate oversized under-plates from the
  previous four-color model were removed.
- Each colored optical insert is 3.28 mm in diameter, 0.16 mm thick, and stays
  0.215 mm below the top of its housing.
- The flamingo and cactus use the two exact filled polygons from the KiCad
  `LOGO` footprint. A 0.16 mm outline strengthening offset protects the
  smallest features for a 0.4 mm nozzle.
- `Secret-DEV-Brain` uses a minimum 0.44 mm stroke and a 0.24 mm raised
  silver layer for legibility.

See `MODEL_VALIDATION.json` for measured bounds, mesh counts, source hashes,
and every D1–D10 center/color assignment.
