# Transformer wiring at J1

> Status: derived 2026-10-18 from the Triad Magnetics VPT24-4170 catalogue sheet
> (published 2021-10-12), the as-built netlist
> (`HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_sch`) and the board. The
> board has never been energised. Confirm the lead map against the physical unit
> (nameplate plus a continuity check) before the first power-up.

## Why this file exists

`J1` takes the off-board VPT24-4170 transformer, and until now the repository
recorded nothing about which lead goes on which terminal. The net names at `J1`
are **lead colours**, and on the Triad sheet the same colours are called "Input"
and "Output" — because the transformer is sold to step 115 V *down* to 12 V. This
board runs it the other way, as a step-up inverter, so the roles are reversed.
Wiring the leads colour-for-colour gives a board that makes about 1.9 V at the
outlet instead of ~120 V. Build from this file instead.

## The part

Triad Magnetics `VPT24-4170`: 100 VA toroidal, dual primary and dual secondary,
eight leads, 9 % regulation typical and a 45 °C temperature rise. The sheet
notes that the windings are designed to be series- or parallel-connected and are
not intended to be used independently.

| Winding | Leads | Series connection | Parallel connection |
| --- | --- | --- | --- |
| Primary (the 115/230 V side) | BLUE, BROWN, GRAY, VIOLET | 230 V: feed BLUE–BROWN, jumper GRAY–VIOLET | 115 V: jumper BLUE–VIOLET and GRAY–BROWN, feed at BLUE and BROWN |
| Secondary (the 12/24 V side) | BLACK, YELLOW, RED, ORANGE | 24 V CT: take BLACK–YELLOW, jumper RED–ORANGE | 12 V: jumper BLACK–ORANGE and RED–YELLOW, take at BLACK and YELLOW |

## What the board expects

`J1` is an eight-pole Wago 2606-1108 block, and each net lands on **two poles** —
the two leads of one parallel-connected winding:

| J1 poles | Net | Role on this board |
| --- | --- | --- |
| 1, 2 | `/BLUE` | H-bridge leg A midpoint (Q3 source / Q5 drain), U5 high-side reference, AIN2 sense divider |
| 3, 4 | `/BROWN` | H-bridge leg B midpoint (Q4 source / Q6 drain), U6 high-side reference, AIN3 sense divider |
| 5, 6 | `/BLACK` | Outlet, J5 poles 1–2 |
| 7, 8 | `/RED` | Outlet, J5 poles 5–6 |

(`J5` is the three-pole outlet block: 1–2 = `/BLACK`, 3–4 = `GND`/earth,
5–6 = `/RED`.)

The bridge runs on the 17 V rail, so it must drive the **12 V winding**; the
outlet must take the **115 V winding**. That gives 17 V × 115/12 ≈ 163 V RMS open
circuit and about 148 V at the full 100 VA load, leaving the headroom the
firmware needs to hold 120 V.

## The connection

| J1 pole | Net | Lead to fit |
| --- | --- | --- |
| 1 | `/BLUE` | BLACK |
| 2 | `/BLUE` | ORANGE |
| 3 | `/BROWN` | RED |
| 4 | `/BROWN` | YELLOW |
| 5 | `/BLACK` | BLUE |
| 6 | `/BLACK` | VIOLET |
| 7 | `/RED` | BROWN |
| 8 | `/RED` | GRAY |

No jumpers on the transformer itself: the two-poles-per-net terminal block *is*
the parallel connection the datasheet specifies (BLACK–ORANGE, RED–YELLOW,
BLUE–VIOLET, GRAY–BROWN), with each pair brought out to the board. Within a pair
either pole will do — the two poles of a net are the same copper — but the pairs
must not be split across nets.

**Do not match colours.** The `/BLUE` net carries the BLACK lead. Fitted
colour-for-colour, the bridge drives the 115 V winding and the outlet gets the
12 V winding, and the inverter makes about 1.9 V.

## Hazards

- Fitted as in the table, the drive side never sees more than the 17 V rail, and
  the outlet side never sees more than ~163 V RMS — which is the side the 1.5 mm
  isolation rule in `Lamoka1-half.kicad_dru` covers.
- Never feed the mains into `J1`. The 12 V winding is a low-voltage winding:
  mains across it puts the H-bridge directly across the line.
- Do not use the 230 V series connection for the outlet. Series-connecting the
  primaries (jumper GRAY–VIOLET, taking the outlet at BLUE and BROWN) raises the
  step-up ratio to 230:12, and the outlet would see about 326 V RMS instead of
  163 V. That is past the 300 V line where IPC-2221B asks 2.5 mm of separation,
  while the board's isolation rule and layout are built for 1.5 mm at 151–300 V.
- `J5`'s earth poles are tied to board `GND`, so board `GND` is protective
  earth. A mis-wire that puts the outlet potential onto the bridge leads shorts
  to earth through the bridge.

## Verify before first power-up

1. Identify the windings on the physical unit by continuity: the 12 V winding is
   the low-resistance one (a 100 VA mains winding is several ohms; the 12 V
   winding is a small fraction of an ohm).
2. Confirm the lead colours against the nameplate or catalogue sheet.
3. Buzz out `J1` to the bridge and to `J5` to confirm the net map above.
4. Record any deviation here.

## The cleaner long-term fix (not done)

Renaming the `J1` nets to their roles (`XFMR_DRIVE_A`, `XFMR_DRIVE_B`, `OUT_L`,
`OUT_N`) would remove the traps above, but those are the net names the
`.kicad_dru` isolation rule and the `.kicad_pcb` key on, so it is a deliberate
schematic-and-board change, not a documentation edit. Do it with a full netlist
and barrier re-verification, never as a side effect.
