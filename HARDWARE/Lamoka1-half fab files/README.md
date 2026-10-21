# Lamoka1-half fab files

Fabrication and assembly submission for the **Lamoka1/2 main board** (power and
control), laid out the same way as
`Lamoka1-DEV-Brain/Lamoka1-DEV-Brain fab files/`. Generated 2026-10-18 with
KiCad 10.0.6 from `HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pcb`;
the green job-file metadata and the interactive BOM were refreshed 2026-10-19.

The folder and its files are named `Lamoka1-half` rather than `Lamoka1/2`
because a slash cannot appear in a path — the same path-safe alias used by
`HARDWARE/Mechanical/Lamoka1-half.step`. The model is the **Lamoka1/2**, and
that model name appears on the silkscreen.

We keep this package committed so anyone can order the board exactly as we did,
without having to regenerate anything first. The board is 95.2 × 95.2 mm of
open hardware, and the whole point is that you can build one yourself.
## Contents

| Path | What it is |
| --- | --- |
| `Lamoka1-half/` | Nine Gerber layers (`F_Cu`, `B_Cu`, `F_Mask`, `B_Mask`, `F_Paste`, `B_Paste`, `F_Silkscreen`, `B_Silkscreen`, `Edge_Cuts`), the Gerber job file, the Excellon drill file, the drill map and the drill report |
| `Lamoka1-half BOM.csv` | PCBWay 9-column assembly BOM: 70 lines covering 116 placements, no blank fields |
| `Lamoka1-half Pick and Place.csv` | 116 placements, 57 front / 59 back (`... top.csv` and `... bottom.csv` are the per-side splits) |
| `Lamoka1-half interactive BOM.html` | Self-contained interactive placement and BOM viewer, generated with [InteractiveHtmlBom](https://github.com/openscopeproject/InteractiveHtmlBom); open it in a browser and click a part to highlight it on the board |
| `README.md` | This file |

The DEV-Brain package also has a `PCBway files/` folder holding the BOM
spreadsheet PCBWay's uploader produced during ordering. That file is created at
order time; add the equivalent here once the board has been ordered.

## The board

- 95.2 x 95.2 mm, 2 copper layers, 1.6 mm FR-4, 1 oz (35 um) outer copper,
  green solder mask and white legend on both sides (from the job file; the
  board renders use the same stackup colour).
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

- 116 placements: 57 front, 59 back.
- Mostly 0402 / 0603 / 0805 / 1206 chip parts, one 2512 current-sense shunt,
  one 3216 tantalum capacitor, two VSON-8 MOSFETs, two SMA diodes, four SMD
  radial electrolytic cans (`Cin1`, `Cout1`-`Cout3`) and one USB-C receptacle
  (`J3`: SMT pads plus a through-hole shield).
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
2. Everything except the secondary runs at 21.75 V or below (11-14 V input,
   21.75 V boost bus, 5 V / 3v3 / 1V1 logic), so the 0.18 mm netclass
   clearance is correct there. The secondary also stays 3.4 mm inside the
   board edge.
3. **Before first power-up** — not a fabrication item, but the same folder will
   be used on the bench: the VPT24-6670 transformer is mounted off-board, and
   the lead-to-terminal map is in `HARDWARE/Transformer-wiring.md`. Read it
   before wiring: the `J1` net names are lead colours, and this transformer's
   BLUE/BROWN leads are its 115 V winding while the bridge sits on the
   `/BLUE`/`/BROWN` nets, so matching colours gives about 1.9 V at the outlet.
   The eight-lead colour map is the same as the 100 VA part it replaces, so
   the harness map does not change — but the harness current capability rises
   and the part is 1.6 kg rather than 1.0 kg, so the enclosure and the
   strain relief have to be re-dimensioned.
4. **The board carries the fixes from the 2026-10-18 electrical reviews.** It
   has the 22 µF tantalum `C13`, the local bypass at `U5`/`U6` pin 7, the
   68 kΩ AIN dividers, the 30 V `Q1`, the 35 V bus capacitors and the 2512
   shunt. Two things from those reviews are **not** on the board: the
   `Rsense1` value stays at 2 mΩ (a 5 mΩ shunt would clip the output to about
   57 W on a 100 W single-phase load), and the fusing is a harness item. The
   `J5` earth cut is made in the board — see the table below. The derivations
   live in `AGENTS.md`, "The power stage".
5. **A third-party schematic review is in the repository.** Tomachie scored the
   design **93/100** on its free pass; the report and a screenshot are in
   `HARDWARE/Kicadfiles/Lamoka1-half/Reviews/`, and the live report is at
   <https://tomachie.com/r/0a968485-5479-41e2-b24f-00a293061b1e/Lamoka1-half_report.html>.
   It carries no AI findings — that pass is a paid add-on and was not bought.
   It does confirm the missing USB ESD protection on `CC1`/`CC2` and the
   USB-C shell, and its remaining deductions are footprint naming and
   test-point coverage, not circuit errors.

## The key design values on this board

Everything below is in the design files in this repository, and this package was
plotted from them:

| Reference | Value |
| --- | --- |
| `Rfbt1`, `Rfbb1` | 64.9 kΩ, 3.79 kΩ — the 21.75 V bus |
| `Rslope1`, `Rcomp1` | 49.9 kΩ, 80.6 kΩ |
| `Ccomp1`, `Ccomp2` | 15 nF, 44.8 pF |
| `R27`, `R29`, `R31` | 68 kΩ each — 25.74 V full scale |
| `Q1` | `CSD17304Q3` (30 V) |
| `Rsense1` | 2 mΩ 2512 2 W (`CSNL2512FT2L00`) |
| `Cout1`–`Cout3` | 35 V 180 µF (`35SVPK180M`) |
| `Coutx1` | 22 µF 1206 35 V (`C3216X5R1V226M160AC`) |
| `C13` | 22 µF 3216 tantalum (`T491A226K010AT`) |
| `C34`/`C36`, `C35`/`C37` | 10 µF 0805 and 100 nF 0603 at each gate driver's VDD pin (four new placements) |
| `J5` pins 3/4 | their own `/PE` net — the outlet earth is not the board's logic ground |

The package has **70 BOM lines covering 116 placements**, and a pick-and-place
of **116 placements, 57 front and 59 back**. The board outline, the drill
table (372 plated and 6 unplated holes) and the via count (322) are
unchanged.

## Known warnings, reviewed and accepted

`kicad-cli pcb drc --schematic-parity` on this board reports **7 DRC warnings,
no errors, no unconnected items, and 14 schematic-parity warnings**:

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

The 14 parity warnings compare the schematic symbols with the board footprints.
All of them are long-standing, none of them affects a fabrication or assembly
file, and the project keeps `footprint_symbol_mismatch`,
`footprint_symbol_field_mismatch` and `net_conflict` at *warning* severity for
exactly this reason. The DEV-Brain, which was fabricated and works, reports 94
of the same kind. They are:

- 12 x `net_conflict` on `U4` — the LM5122 footprint carries twelve `V`
  through-hole thermal-via pads inside the PowerPAD. The symbol deliberately
  has no such pins; the pads are part of the footprint's thermal design.
- 2 x "Exclude from bill of materials settings differ" (`J4`, `J6`) — the board
  footprints carry the exclude-from-BOM attribute that keeps the fan wire pads
  out of the BOM and CPL. The schematic symbols do not, because the schematic
  is also the reference of what is drawn.

(Three further warnings — "Missing symbol field 'MPN' in footprint" on `J1`,
`J2` and `J5` — are resolved: the MPN data now lives on the board
footprints as well as on the schematic symbols.)

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

The interactive BOM is generated with the upstream
[InteractiveHtmlBom](https://github.com/openscopeproject/InteractiveHtmlBom)
plugin, which is not vendored here. From the board directory:

```sh
cd HARDWARE/Kicadfiles/Lamoka1-half
INTERACTIVE_HTML_BOM_NO_DISPLAY=1 \
python3 /path/to/InteractiveHtmlBom/InteractiveHtmlBom/generate_interactive_bom.py \
  Lamoka1-half.kicad_pcb --no-browser \
  --dest-dir "../../Lamoka1-half fab files" \
  --name-format "Lamoka1-half interactive BOM" \
  --include-nets --include-tracks --checkboxes all
```

The zone fills stored in the `.kicad_pcb` are part of the plot, not recomputed
at export time: always refill before running the export above.
