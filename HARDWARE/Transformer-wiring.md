# Transformer wiring at J1

> Read this before you connect the transformer. The board has never been
> energised, so confirm the lead map against the physical unit (nameplate plus
> a continuity check) before the first power-up.

## Why this file exists

`J1` takes the off-board transformer, and the net names there are **lead
colours**. On the Triad catalogue sheet those same colours are called "Input"
and "Output" — because the transformer is sold to step 115 V *down* to 12 V.
This board runs it the other way, as a step-up inverter, so the roles are
reversed.

Wiring the leads colour-for-colour gives you a board that makes about 1.5 V at
the outlet instead of ~120 V. Build from this file instead.

## The part

Triad Magnetics `VPT24-6670`: 160 VA toroidal, dual primary and dual
secondary, eight leads, 8 % regulation typical (from full load to no load),
89 % efficiency at full load, a 50 °C temperature rise, 4 kV hipot and a Class
B (130 °C) insulation system. The sheet notes that the windings are designed to
be series- or parallel-connected and are not intended to be used
independently.

| Property | Value |
| --- | --- |
| Maximum power | 160 VA |
| Input | Series 230 VAC / Parallel 115 VAC, 50/60 Hz |
| Output | Series 24 VAC CT @ 6.67 A / Parallel 12.0 VAC @ 13.34 A |
| Regulation | 8 % TYP from full load to no load (no maximum published) |
| Temperature rise | 50 °C TYP |
| Hipot | 4000 VAC, primary to secondary |
| Efficiency | 89 % TYP at full load |
| Insulation | Class B (130 °C) |
| Mechanical | Ø106 × 52 mm, 1.6 kg, M6 × 60 mm bolt, 8 in UL 1015 leads (primaries 22 AWG, secondaries 16 AWG) |

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

(`J5` is the three-pole outlet block: 1–2 = `/BLACK`, 3–4 = the earth pole
on its own `/PE` net, 5–6 = `/RED`.)

The bridge runs on the **21.75 V bus**, so it must drive the **12 V winding**;
the outlet must take the **115 V winding**. At the PWM dead-time clamp
(`m = 0.959733`) the drive winding sees about 14.8 V RMS on a nominal bus, and
the transformer's true no-load ratio (115 / 12.96 = 8.8735) puts the no-load
outlet at about **131 V**. At the top of the bus tolerance band it reaches
about 135 V. The working ceiling is the firmware's own `OUTLET_V_MAX` = 125 V
on the estimated outlet.

The 12.0 V nameplate is a **full-load** rating. The open-circuit secondary is
12.0 × 1.08 = 12.96 V, so the true no-load turns ratio is 8.8735, not
`115/12 = 9.583`. The firmware uses the no-load ratio (and the series
impedance referred to the 115 V side, 5.666 Ω) to estimate the outlet. Both are
the transformer's catalogue values, used as they come — no unit has to be
fitted or calibrated.

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
12 V winding, and the inverter makes about 1.5 V.

## Nothing measures the output

> [!CAUTION]
> There is no voltage or current sensing on the 115 V side of this board. The
> firmware regulates an estimate of the outlet, and the only way to know what
> the transformer is really doing is to measure it yourself. Treat everything
> past `J1` as live mains.

## Hazards

- Fitted as in the table, the drive side never sees more than the 21.75 V bus,
  and the outlet side never sees more than about 135 V RMS — which is the side
  the 1.5 mm isolation rule in `Lamoka1-half.kicad_dru` covers.
- **60 Hz only.** 138 V RMS at 60 Hz on the 115 V winding is 100 % of this
  transformer's design flux (the design point is 230 V at 50 Hz). At 50 Hz the
  same outlet voltage would over-excite the core, so the firmware must not
  offer a 50 Hz mode.
- Never feed the mains into `J1`. The 12 V winding is a low-voltage winding:
  mains across it puts the H-bridge directly across the line.
- Do not use the 230 V series connection for the outlet. Series-connecting the
  primaries (jumper GRAY–VIOLET, taking the outlet at BLUE and BROWN) doubles
  the step-up ratio, so the no-load outlet would be about 262 V RMS instead of
  about 131 V — a 370 V peak, past the 300 V line where IPC-2221B asks 2.5 mm
  of separation, while the board's isolation rule and layout are built for
  1.5 mm at 151–300 V.
- **The outlet's earth pole.** `J5` pins 3/4 sit on their own `/PE` net, not on
  board `GND`. Board `GND` is not the outlet earth, and the 115 V secondary has
  no earth of its own until the harness provides one: bond `J5` 3/4 to a chassis
  PE stud (M3) for a metal enclosure (Class I), or leave it unbonded and mark
  the unit double-insulated (Class II) for a plastic one. A three-prong outlet
  must never ship with its earth floating or tied to the board's logic ground.

## Verify before first power-up

1. Identify the windings on the physical unit by continuity: the 12 V winding is
   the low-resistance one (a 160 VA mains winding is several ohms; the 12 V
   winding is a small fraction of an ohm).
2. Confirm the lead colours against the nameplate or catalogue sheet.
3. Buzz out `J1` to the bridge and to `J5` to confirm the net map above.
4. With the transformer disconnected from the board, apply **115.00 V RMS at
   60 Hz** to the 115 V winding and measure the **open-circuit** 12 V winding.
   About 12.96 V confirms the full-load convention and the no-load ratio of
   8.8735; about 12.00 V would mean the other convention and every outlet
   number would shift.
5. Record any deviation here.

## The cleaner long-term fix (not done)

Renaming the `J1` nets to their roles (`XFMR_DRIVE_A`, `XFMR_DRIVE_B`, `OUT_L`,
`OUT_N`) would remove the traps above, but those are the net names the
`.kicad_dru` isolation rule and the `.kicad_pcb` key on, so it is a deliberate
schematic-and-board change, not a documentation edit. Do it with a full netlist
and barrier re-verification, never as a side effect.
