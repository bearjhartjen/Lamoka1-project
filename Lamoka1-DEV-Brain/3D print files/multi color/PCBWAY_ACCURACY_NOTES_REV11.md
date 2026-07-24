# PCBWAY ACCURATE REV11 decisions

## Geometry retained

The starting point is the exact repository multicolor OBJ. Blue and
red meshes are byte-for-geometry unchanged. The original black PCB
and component mesh—including its real drilled bores—is unchanged.
The original silver geometry is retained except for the overly
tessellated J1 export and print artifacts listed below.

## J1 source of truth

- PCB footprint attribute: through-hole, excluded from BOM, and
  excluded from position files.
- PCBWay BOM: no J1 row.
- PCBWay pick-and-place: no J1 row.
- PCBWay quotation: no J1 line item.
- Production drill report: exactly thirteen 1.000 mm PTH holes.
- KiCad pads: 1.70 mm, with square pin 1 and round pins 2–13.

The replacement geometry keeps the 1.00 mm bores and uses the real
2.20/2.00 mm solder-mask openings for the visible silver boundary.
A 0.16 mm top relief is deliberately added because a co-planar,
overlapping silver ring was being hidden by the black PCB volume in
the slicer. There is no connector body and there are no projecting
pins.

## Removed source islands

- Overly tessellated source J1 shells replaced:
  26
- 0.04 mm sub-layer sheets removed:
  207
- Detached sub-nozzle slivers removed:
  4
- Original silver component shells retained:
  104

These removals target the silver flecks and strings seen in the
physical print without simplifying the recognizable board.
