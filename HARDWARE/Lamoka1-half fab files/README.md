# Lamoka1-half fab files

Fabrication and assembly submission for the **Lamoka1/2 main board** (power and
control), laid out the same way as
`Lamoka1-DEV-Brain/Lamoka1-DEV-Brain fab files/`. Generated 2026-10-18 with
KiCad 10.0.6 from `HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pcb`.

The folder and its files are named `Lamoka1-half` rather than `Lamoka1/2`
because a slash cannot appear in a path — the same convention as
`HARDWARE/Mechanical/Lamoka1-half.step`. The board itself is the Lamoka1/2 in
prose and on its silkscreen.

## Contents

| Path | What it is |
| --- | --- |
| `Lamoka1-half/` | Nine Gerber layers (`F_Cu`, `B_Cu`, `F_Mask`, `B_Mask`, `F_Paste`, `B_Paste`, `F_Silkscreen`, `B_Silkscreen`, `Edge_Cuts`), the Gerber job file, the Excellon drill file, the drill map and the drill report |
| `Lamoka1-half BOM.csv` | PCBWay 9-column assembly BOM: 70 lines covering 112 placements, no blank fields |
| `Lamoka1-half Pick and Place.csv` | 112 placements, 57 front / 55 back (`... top.csv` and `... bottom.csv` are the per-side splits) |
| `README.md` | This file |

The DEV-Brain package also has a `PCBway files/` folder holding the BOM
spreadsheet PCBWay's uploader produced during ordering. That file is created at
order time; add the equivalent here once the board has been ordered.

## The board

- 95.2 x 95.2 mm, 2 copper layers, 1.6 mm FR-4, 1 oz (35 um) outer copper,
  white solder mask and black legend on both sides (from the job file).
  The finish is unset in the job file ("None") — HASL or lead-free HASL is the
  usual choice, select it in the order form.
- 378 holes. Plated, smallest first: 0.2032 mm x12 (the LM5122 PowerPAD thermal
  vias), 0.30 mm x322, 0.60 mm x4 (these four are slots), 1.10 mm x12,
  1.20 mm x4, 1.30 mm x6, 2.00 mm x12. Unplated: 0.65 mm x2, 1.80 mm x2 (the
  switch legs), 3.20 mm x2 (M3 mounting holes).
- 322 vias: 242 at 0.60/0.30 mm (0.15 mm annular ring, inside PCBWay's
  standard) and 80 at 0.50/0.30 mm (0.10 mm ring). PCBWay publishes 0.10 mm as
  its 4 mil "medium difficulty" class for 1 oz outer copper. The 0.50/0.30 mm
  vias are not an accident — the control area has no room for a wider pad —
  so if a quote engine complains, choose the appropriate process tier rather
  than letting it enlarge the vias.
- No fiducials. Assembly registration relies on the board outline and the
  connector pads, as on the DEV-Brain board.
- The Gerber job file carries `Revision: "rev?"`, inherited from the DEV-Brain
  job file. It is a free-text label rather than a design revision — set it in
  the order form if the board is ever revised, and leave it otherwise.

## Assembly

- 112 placements: 57 front, 55 back.
- Mostly 0402 / 0603 / 0805 / 1206 chip parts, two VSON-8 MOSFETs, two SMA
  diodes, four SMD radial electrolytic cans (`Cin1`, `Cout1`-`Cout3`) and one
  USB-C receptacle (`J3`: SMT pads plus a through-hole shield).
- Through-hole / hand-fit lines in the BOM: `J1` (transformer), `J2`, `J5`
  (Wago terminals), `SW1` (button) and `Q3`-`Q6` (TO-220 H-bridge MOSFETs).
- Deliberately absent from the BOM and CPL, because they are board features
  rather than ordered parts: `H1`/`H2` (M3 mounting holes), `J4`/`J6` (fan wire
  solder pads) and `NT1` (net tie).

## Ordering notes

1. **Isolation rule.** The 120 V transformer secondary (`/BLACK`, `/RED`) is
   held 1.5 mm clear of everything else by a custom rule in
   `HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_dru`. IPC-2221B table
   6-1 column B2 asks for 1.25 mm at 151-300 V working, so 1.5 mm has margin.
   Zones were refilled after the last copper edit — if the board is edited
   again, refill the zones (`B` in pcbnew) or the plotted pour will not obey
   the rule. The stored fills were checked against a fresh refill before this
   package was plotted.
2. Everything except the secondary runs at 17 V or below (11-14 V input, 17 V
   boost rail, 5 V / 3v3 / 1V1 logic), so the 0.18 mm netclass clearance is
   correct there. The secondary also stays 3.4 mm inside the board edge.
3. **Before first power-up** — not a fabrication item, but the same folder will
   be used on the bench: the VPT24-4170 transformer is mounted off-board, and
   the lead-to-terminal map is in `HARDWARE/Transformer-wiring.md`. Read it
   before wiring: the `J1` net names are lead colours, and this transformer's
   BLUE/BROWN leads are its 115 V winding while the bridge sits on the
   `/BLUE`/`/BROWN` nets, so matching colours gives about 1.9 V at the outlet.
4. **Read the 2026-10-18 electrical reviews before ordering.** Together they
   recommend a minimum change set for the board itself: `C13` to 22 µF, a supply
   bypass at `U5`/`U6` pin 7, `Rsense1` re-derived from 2 mΩ, and a fusing
   decision. The transformer wiring above is the fifth item, and that one is
   documented rather than changed. Nothing in this folder reflects those
   proposals yet — as it stands the package matches the board as designed and
   drawn. The reviews' bench and firmware items (plug-in inrush before the first
   battery connection, dead time, start-up sequencing) live in the same section
   of `AGENTS.md`, "Open findings from the 2026-10-18 reviews".

## Known warnings, reviewed and accepted

`kicad-cli pcb drc --schematic-parity` on this board reports **7 DRC warnings,
no errors, no unconnected items, and 17 schematic-parity warnings**:

- 2 x silkscreen clipped by the board edge — two segments of `J3`'s outline
  cross the edge because the USB-C receptacle overhangs it. Cosmetic.
- 4 x dangling GND vias and 1 isolated `1V1` copper fragment under `U1` — the
  deliberate keepout over the MCU's thermal pad stops the pour there, so these
  vias sit in a region with no plane on either layer. Design intent, already
  locally overridden in the board file.
- 40 % of the visible silk legends are 0.40-0.75 mm high, below PCBWay's
  0.8 mm legibility guideline. This matches the DEV-Brain board (51 % below
  0.8 mm), which was fabricated and assembled without trouble; the dense areas
  have no room for larger text, and the connector references plus the
  "DANGER! HIGH VOLTAGE" legend are 1.0-1.6 mm.

The 17 parity warnings compare the schematic symbols with the board footprints.
All of them are long-standing, none of them affects a fabrication or assembly
file, and the project keeps `footprint_symbol_mismatch`,
`footprint_symbol_field_mismatch` and `net_conflict` at *warning* severity for
exactly this reason. The DEV-Brain, which was fabricated and works, reports 94
of the same kind. They are:

- 12 x `net_conflict` on `U4` — the LM5122 footprint carries twelve `V`
  through-hole thermal-via pads inside the PowerPAD. The symbol deliberately
  has no such pins; the pads are part of the footprint's thermal design.
- 3 x "Missing symbol field 'MPN' in footprint" (`J1`, `J2`, `J5`) — MPN data
  lives on the schematic symbols, which is what feeds the assembly BOM. The
  board footprints do not need to repeat it.
- 2 x "Exclude from bill of materials settings differ" (`J4`, `J6`) — the board
  footprints carry the exclude-from-BOM attribute that keeps the fan wire pads
  out of the BOM and CPL. The schematic symbols do not, because the schematic
  is also the reference of what is drawn.

A parity check that is not asked for reports nothing: `kicad-cli pcb drc`
without `--schematic-parity` prints "0 Footprint errors" and hides all of the
above. Always pass the flag (as `CONTRIBUTING.md` documents).

## Regenerating this folder

```sh
PCB=HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pcb
SCH=HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_sch
OUT="HARDWARE/Lamoka1-half fab files"

kicad-cli pcb export gerbers --no-protel-ext \
  --layers "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts" \
  --output "$OUT/Lamoka1-half/" "$PCB"

kicad-cli pcb export drill --drill-origin absolute --excellon-units mm \
  --excellon-zeros-format decimal --excellon-min-header \
  --generate-map --map-format gerberx2 --generate-report \
  --output "$OUT/Lamoka1-half/" "$PCB"

kicad-cli pcb export pos --format csv --units mm --side both \
  --output "$OUT/Lamoka1-half Pick and Place.csv" "$PCB"
kicad-cli pcb export pos --format csv --units mm --side front \
  --output "$OUT/Lamoka1-half Pick and Place top.csv" "$PCB"
kicad-cli pcb export pos --format csv --units mm --side back \
  --output "$OUT/Lamoka1-half Pick and Place bottom.csv" "$PCB"

kicad-cli sch export netlist --output /tmp/Lamoka1-half.net "$SCH"
python3 tools/make_fab_bom.py /tmp/Lamoka1-half.net "$OUT/Lamoka1-half BOM.csv"
```

The zone fills stored in the `.kicad_pcb` are part of the plot, not recomputed
at export time: always refill before running the export above.
