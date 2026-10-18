# HARDWARE/Mechanical

Mechanical models for the Lamoka1 project. Everything in this folder is an
export: the KiCad layouts are the sources, and every file here can be
regenerated from them.

| File | What it is |
| --- | --- |
| `Lamoka1-half.step` | Lamoka1/2 main board — outline, holes, and component bodies |
| `Lamoka1-DEV-Brain.step` | DEV-Brain control board — outline, holes, and component bodies |

## Board models

The two board STEP models are exports of the KiCad layouts, so regenerate them
after any layout change:

```bash
make step        # both boards, via kicad-cli
```

Component bodies come from the vendor 3D models that the layouts reference
under `${KIPRJMOD}/components/3dmodels/`. Those files are not redistributed
(see [License](../../README.md#license)), so a fresh clone exports the bare
board until you supply your own copies — the export itself always works, only
the component bodies depend on them.

All of it is covered by the project's hardware license, CERN-OHL-W-2.0, like
the rest of `HARDWARE/`.
