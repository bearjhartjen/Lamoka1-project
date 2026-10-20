# Lamoka1/2 Rev B and Rev C change sets

> Status: **Rev B is found and decided; Rev C is applied.**
> The board in `HARDWARE/Kicadfiles/Lamoka1-half/` carries the Rev C changes:
> the 21.75 V boost bus, the 68 kΩ AIN dividers, the 30 V `Q1`, the 2512
> shunt, the 22 µF tantalum `C13`, the `U5`/`U6` VDD bypass and the 35 V bus
> capacitors. The firmware in `SOFTWARE/Portal Interface Engine/V1.9.ino` is
> on the same operating point.
>
> **The board has never been energised.** Nothing here authorises building or
> powering it. The harness items below — the output filter, the fuses, the
> reverse-polarity protection, the earth bond — are still open. The `J5` earth
> cut is a board change and it is now in the KiCad sources: `J5` pins 3/4 are
> on their own `/PE` net, not on `GND`. See the status column in the change
> table.
>
> This file is now a **record**: what the 2026-10-18 electrical reviews found,
> the derivations behind each item, which of them were applied, and what is
> still open. The derivations are kept because the next revision has to
> re-derive them rather than trust a summary.

## Change table

| Reference | Before (Rev B baseline) | After (Rev C, as fitted) | Status | Reason |
| --- | --- | --- | --- | --- |
| Off-board output filter, J5 → outlet | none | Series L 38 mH min, 1.5 A sat, ≥250 VAC insulation, <0.5 Ω DCR; shunt C 470 nF, ≥630 VDC/250 VAC polypropylene; series RC damper 100 Ω/1 W + 100 nF/630 VDC; bleeder 1 MΩ/1 W | **Open** — harness | 20 kHz PWM residue and switching edges at the outlet; fc ≈ 1.2 kHz, ≈ 49 dB at 20 kHz |
| Off-board output protection | none | 1.25 A/250 VAC slow-blow fuse in series with the outlet; 150 VAC MOV at the outlet | **Open** — harness | Output is unfused; 0.83 A at 120 V and 100 W, so 1.25 A gives 1.5× at full load; verify the slow-blow carry |
| Input harness, J2 | 2-pole connector only | 15 A blade fuse; reverse-polarity protection; TVS; 12 AWG cable | **Open** — harness | 100 W at 11 V/0.85 = 10.7 A; no fuse or reverse protection today |
| `R27` (AIN1 bus) | 43.2 kΩ 0603 `RC0603FR-0743K2L` | **68 kΩ 0603 `RC0603FR-0768KL`** | **Applied** | AIN1 full scale 17.56 V → 25.74 V; the 21.75 V bus sits at 2.79 V, not near the 3.3 V clip |
| `R29` (AIN2), `R31` (AIN3) | 45.3 kΩ 0603 `RC0603FR-0745K3L` | **68 kΩ 0603 `RC0603FR-0768KL`** | **Applied** | The old value clipped at 18.25 V, so the 21.75 V bus saturated the ADC and the firmware read 74 % of the truth |
| `DIV_AIN1`/`2`/`3` (firmware) | `10/(43.2+10)`, `10/(45.3+10)` | **`10/(68+10)`**, bus windows **±0.90 V start / ±1.5 V run** | **Applied** | Matches the new dividers. The old ±0.4 V windows existed only to stay under the old clip |
| `U5`/`U6` pin 7 (VDD) | no local bypass; VDD is CSP | **10 µF 0805 `GRM21BZ71E106KE15L` (`C34`, `C36`) + 100 nF 0603 `CL10B104KB8NNNC` (`C35`, `C37`) at each pin to GND** | **Applied** | VDD is the 11–14 V CSP rail, not the 21.75 V bus; gate-driver switching current had no local reservoir |
| `D11`/`D12` bootstrap diodes | SS14, anode CSP, cathode HB1/HB2 | no change; verify the same orientation on the board | **Verified** | Already correct in the netlist (D11/D12 pin 2 = A on CSP, pin 1 = K on HB1/HB2); recorded as verified, not redesigned |
| `C13` | 10 µF 0402 MLCC `CL05A106MQ5NUNC` | **22 µF tantalum A case `T491A226K010AT`** | **Applied** | AMS1117 compensation expects 22 µF with ESR; near-zero-ESR MLCC is the risky condition |
| `Rsense1` | 2 mΩ 1206 `CSNL1206FT2L00` | **2 mΩ 2512 2 W `CSNL2512FT2L00`** (Stackpole) | **Applied — value unchanged, package changed** | The 5 mΩ proposal is **wrong**: a 100 W single-phase load has a ~27 A inductor crest, so 5 mΩ would clip the output to about 57 W. The package moves for thermal reasons only |
| `J5` pins 3/4 | tied to board GND | **cut from GND; own `/PE` net**, bonded in the harness to a chassis PE stud (M3) for Class I, or left unbonded and the unit marked double-insulated for Class II | **Applied** | Board GND is not protective earth; the old tie made every GND point on the board an earth conductor |
| `U8`/`U9` (TMP235) | U8 in control section, U9 in inversion section | Keep U8; add thermal vias to Q1/Q2 or derate; consider moving U9 closer to Q4/Q6 | **Open** | U9 is 10.3 mm from Q6 but 39.7 mm from Q5; U8 is 32–39 mm from the bridge. 85 °C trip is board temperature, not junction |
| `Q1` | `CSD16323Q3`, 25 V, DQG0008A | **`CSD17304Q3`, 30 V, DQG0008A** (the same part as `Q2`) | **Applied** | 25 V on a 21.75 V switching node leaves about 3 V for ringing; 30 V leaves about 8 V. No 40 V part exists in this footprint |
| `Rfbt1` | 49.9 kΩ `CRCW040249K9FKED` | **64.9 kΩ `CRCW040264K9FKED`**, `Rfbb1` 3.79 kΩ unchanged | **Applied** | `1.2 × (1 + 64.9/3.79) = 21.749 V` — the LM5122's 1.2 V feedback reference sets the bus |
| `Rslope1`, `Rcomp1`, `Ccomp1`, `Ccomp2` | 105 kΩ, 28.7 kΩ, 3.9 nF, 130 pF | **49.9 kΩ `RC0201FR-0749K9L`, 80.6 kΩ `RC0201FR-0780K6L`, 15 nF `CGA4F1C0G2A153J085AE`, 44.8 pF `GRM1555C1H448JA01D`** | **Applied** | Re-derived for the 21.75 V operating point; cross-over about 9.3–11.8 kHz with roughly 80° of phase margin |
| `Cout1`–`Cout3` | 25 V 180 µF `25SVPF180M` | **35 V 180 µF `35SVPK180M`**, same 8 × 11.9 mm land pattern | **Applied** | The 25 V parts ran at 89.5 % of their rating at the top of the new bus band |
| `Coutx1` | 47 µF 1206 | **22 µF 1206 35 V `C3216X5R1V226M160AC`** | **Applied** | A 47 µF 1206 at 35 V does not exist |
| Transformer | `VPT24-4170`, 100 VA, 9 % regulation | **`VPT24-6670`, 160 VA, 8 % regulation, 12.0 V @ 13.34 A** | **Applied** | Same eight-lead colour map, so the `J1` harness map does not change; 100 W is 60 % of the transformer instead of over 100 % |

## Calculations

### 1. Output filter (still open)

- `fc = 1/(2π√(LC)) = 1/(2π√(38 mH × 470 nF)) = 1.19 kHz`.
- Attenuation at 20 kHz, second order: `40 log10(20/1.19) = 49.0 dB`.
- The 49 dB figure assumes an ideal inductor. Measure the 38 mH part's
  self-resonance and the transformer leakage; if the self-resonance is below
  about 200 kHz, the 20 kHz attenuation is set by parasitic capacitance, not
  the 38 mH. A segmented/gapped winding or a two-stage filter may be needed.
- Characteristic impedance `Z0 = √(L/C) = √(38 mH/470 nF) = 284 Ω`.
- Damping at 100 W: `R = 120²/100 = 144 Ω`, `ζ = Z0/(2R) = 0.99`.
  This is essentially critical damping; `ζ` reaches 1.0 at about 100 W. There
  is no resonant peaking at full load.
- Damper corner: `1/(2π × 100 Ω × 100 nF) = 15.9 kHz`; the 100 nF blocks DC, so
  the 100 Ω does not dissipate at 60 Hz.
- Bleeder: `τ = 1 MΩ × 470 nF = 0.47 s`. The working outlet ceiling is the
  firmware's own `OUTLET_V_MAX` = 125 V, a 177 V peak; it decays to below 50 V
  in about 0.59 s.
- Inductor DC loss at 0.83 A (120 V, 100 W): `0.83² × 0.5 Ω = 0.35 W`. The 1 W
  bleeder sees `177²/1 MΩ = 0.031 W`.
- The 470 nF shunt is an across-the-line capacitor. A 250 VAC polypropylene
  part is adequate at 120 V, but an X2 (275 VAC) part is preferred for
  agency approval. The 630 VDC rating covers the 177 V peak with margin.

### 2. Input harness (still open)

- Worst-case input current: `100 W/(11 V × 0.85) = 10.7 A`; 15 A/10.7 A = 1.40.
- 12 AWG has roughly 20 A bundled / 30 A free-air ampacity; verify against the
  chosen insulation and bundle. At 10.7 A the drop is small.
- Reverse-polarity P-channel MOSFET: ≥30 V, ≤10 mΩ at the available gate drive,
  with a 15 V gate-source clamp. At 10 mΩ the loss is `10.7² × 0.01 = 1.15 W`.
- Schottky alternative: at 0.5 V forward, `10.7 × 0.5 = 5.4 W`, so it needs a
  heatsink; the MOSFET is preferred.
- TVS: choose a standoff above the 14 V maximum and a clamp below Q1's rating.
  A 15 V standoff part clamps around 24 V; with `Q1` now a 30 V part the
  margin is about 6 V, where it was about 1 V. Verify the surge clamp at the
  actual test current, not just the datasheet headline.

### 3. AIN dividers (applied)

- `DIV_AIN1 = 10/(68 + 10) = 0.128205`, the same for AIN2 and AIN3.
- Full scale: `3.3/0.128205 = 25.74 V`; the 21.75 V bus reads 2.79 V.
- Divider current: `3.3 V/78 kΩ = 42 µA`; each 68 kΩ resistor dissipates
  `(25.74 − 3.3)²/68 kΩ = 7.4 mW`.
- The old 43.2 kΩ bus divider clipped at `3.3 × 53.2/10 = 17.56 V` and the old
  45.3 kΩ midpoint dividers clipped at 18.25 V. Both are below the Rev C bus,
  which is why they had to change before the bus was raised — a clipped reading
  would have passed as a valid bus voltage.
- The windows are **±0.90 V at start and ±1.5 V while running**. The start
  window has to accept the whole ±0.63 V tolerance band plus the reference
  tolerance; the run window also has to absorb the load-dump droop and the
  120 Hz bus ripple.

### 4. Gate-driver bypass (applied)

- VDD is `/Inversion-Section/CSP`, 11–14 V, confirmed in the netlist for U5
  pin 7, U6 pin 7, U5/U6 pin 4 and the D11/D12 anodes. The 21.75 V bus is not
  the driver supply.
- Datasheet rule: `CVDD ≥ 10 × CBOOT`. C22/C23 are 1 µF bootstrap capacitors,
  so the strict value is 10 µF, not 1 µF. Each driver now carries a 10 µF
  0805 (`C34`, `C36`) and a 100 nF 0603 (`C35`, `C37`) at pin 7, with a short
  return to the pin 5 GND. The nearest bulk capacitor used to be 49–84 mm away.

### 5. C13 / AMS1117-3.3 output (applied)

- AMS1117 requires about 22 µF on the output, and the ESR is part of its
  compensation. A 10 µF 0402 MLCC has essentially zero ESR and is the unstable
  condition.
- `T491A226K010AT` is 22 µF, 10 V, ±10 %, A case (3216), with about 3.2 Ω of
  ESR. At 3.3 V the 10 V rating gives 3× margin. This is a stability fix.
- Alternative if a polarised part is unwelcome: 22 µF X5R 1206 in series with
  1 Ω, which gives a defined ESR. Do not fit a bare 22 µF MLCC.

### 6. Rsense1 — the 5 mΩ finding was wrong

- `IPEAK(CL) = 75 mV/RS`. At 2 mΩ that is **37.5 A typical**; the guaranteed
  threshold is 65.5 mV over −40…125 °C, so **32.75 A minimum** and 87.5 mV
  gives 43.75 A maximum.
- The earlier finding proposed 5 mΩ (the LM5122 procedure wants 4.1–5.6 mΩ)
  because a 2 mΩ shunt lets the inductor saturate before the limit trips. That
  is **wrong for this design**. A 100 W single-phase load draws bus current
  that pulsates at 120 Hz between zero and twice its average, so the inductor
  current carries a crest of about 27 A on top of the switching ripple. A 5 mΩ
  shunt would clamp the current at 15 A and clip the output to about 57 W.
- At 2 mΩ the crest has 1.29× margin against the guaranteed-minimum limit and
  1.48× against the typical one, and the inductor never reaches its
  saturation class.
- The **package** changes anyway, for thermal reasons. The 1206 part is rated
  1 W at 80 °C and derates linearly; at a 120 °C ambient it can do about
  0.556 W, and the shunt dissipates about 0.38 W — 68 % of its derated rating.
  The 2512 2 W part is rated 2 W at 70 °C and derates to 1.0 W at 120 °C, so
  the same 0.38 W is **38 %**. The part is `CSNL2512FT2L00`, made by
  **Stackpole** — the earlier BOM credited Bourns, which was wrong.
- The move from 1206 to 2512 is a footprint change, and the part is a
  two-terminal device, so **no Kelvin sensing** is implemented: the sense
  returns share the shunt's pads.
- The compensation was re-derived for the new operating point anyway, because
  the operating point moved: `Rslope1` 105 kΩ → 49.9 kΩ, `Rcomp1` 28.7 kΩ →
  80.6 kΩ, `Ccomp1` 3.9 nF → 15 nF, `Ccomp2` 130 pF → 44.8 pF.

### 7. J5 earth (applied)

- Netlist: J5 pins 3/4 are on their own `/PE` net; J5 pins 1/2 are `/BLACK`
  and 5/6 are `/RED`. Board `GND` is therefore no longer the outlet earth —
  the Rev C cut is in both the schematic and the board. On the board, pads 3
  and 4 are joined by a 2.40 mm track matching the `/BLACK` and `/RED` pole
  links, and the GND pour was refilled and pulled back from them.
- Class II (double-insulated enclosure): leave the outlet earth pin unbonded,
  mark the outlet and the enclosure, and do not rely on board GND for
  protection. The 1.5 mm secondary barrier remains the only separation.
- Class I: the 3/4-to-GND tie is already cut on the board; bond J5 3/4 to a
  dedicated PE stud (M3) in the harness with a low-inductance connection, and
  keep the 1.5 mm barrier. Never use board GND as the PE conductor.
- The enclosure class is a mechanical decision not recorded in the repository;
  choose it before the harness is built.
- **A three-prong outlet must never ship with its earth left floating or tied
  to the board's logic ground.** The harness provides the bond; the board
  provides only the isolated secondary.

### 8. Thermal monitoring (still open)

- Measured placement: U9 is 10.3 mm from Q6, 29.1 mm from Q3, 39.7 mm from Q5;
  U8 is 32.1 mm from Q6 and 32.8 mm from Q3.
- The TMP235s report board temperature. The 85 °C trip therefore protects the
  board, not the FET junction; a TO-220 or DQG0008A junction can be much hotter
  than the board at the same instant.
- Q1/Q2 are DQG0008A with **zero thermal vias** (checked in the board file).
  Q3–Q6 are TO-220-3 vertical with no heatsink.
- Options: add a thermal via array under Q1/Q2 pads 9 (and connect to the
  GND/copper pour), or derate the continuous output. Consider moving U9 closer
  to Q4/Q6, the two devices farthest from it. Record the resulting junction
  temperature budget; it is not in the repository today.

### 9. Q1 voltage margin (applied)

- Q1 was `CSD16323Q3`, 25 V, on the switching node. At the Rev C bus of
  21.75 V (22.39 V at the top of the tolerance band) that leaves about 3 V for
  leakage-inductance ringing at ~495 kHz with no snubber.
- Q2 is already `CSD17304Q3`, 30 V, in the same DQG0008A footprint, so the
  lowest-risk change is to use the same 30 V part for Q1: about 8 V of
  headroom at the nominal bus and 7.6 V at the worst corner. **No 40 V part
  exists in that footprint**, so 30 V is the ceiling for this land pattern.
- Raising Q1 to 30 V also improves the input TVS clamp margin (see item 2).

### 10. Transformer (applied)

- The fitted part is now the Triad `VPT24-6670`: 160 VA toroidal, 115 V
  primary, 12.0 V @ 13.34 A secondary on the parallel connection, 8 %
  regulation typical, 89 % efficiency at full load, 50 °C temperature rise,
  4 kV hipot, Class B insulation, Ø106 × 52 mm, 1.6 kg, an M6 × 60 mm bolt
  and 8 in UL 1015 leads (22 AWG primaries, 16 AWG secondaries).
- The board drives the **12 V winding** and takes the outlet from the **115 V
  winding**; the lead map is `HARDWARE/Transformer-wiring.md` and must not be
  fitted colour-for-colour. The `-6670`'s own datasheet gives the same eight
  lead colours and the same jumper scheme as the `-4170` it replaces, so the
  `J1` harness map does not change — but the harness current capability
  rises and the unit is 1.6 kg rather than 1.0 kg, so the enclosure has to be
  re-dimensioned.
- 100 W at 89 % transformer efficiency is about 111 VA through the
  transformer. That is 111 % of a 100 VA rating, which is why the part
  changed: the 160 VA part runs at 69 % VA, and the 12 V winding's 13.34 A
  rating puts 100 W at 60 % of the winding instead of over 100 %.
- **The transformer's regulation is 8 % TYP with no published limit.** At
  100 % of the 12 V winding's rating the no-load secondary is 12.0 × 1.08 =
  12.96 V, so the true no-load turns ratio is `115 / 12.96 = 8.8735`. The
  series impedance referred to the 115 V side is
  `Z1 = (12.96 − 12.0)/13.34 × 8.8735² = 5.666 Ω`. Both are catalogue
  numbers; the firmware fits them per unit at bring-up.
- The drive the transformer needs to hold 120.0 V at 100 W is about 14.06 V
  RMS, and the modulation that takes is `m = 14.06 × √2 / 21.75 = 0.914`. At
  the bottom of the bus band (21.13 V) it is `m = 0.941`, which leaves about
  2 % against the `PWM_M_MAX` clamp of 0.959733 — and only if the fitted
  transformer really regulates 8 %. A unit that regulates 10 % needs
  `m = 0.966` at that corner, which the clamp rejects. **The bring-up must
  measure the real transformer.**

## Rev C (applied)

Rev C is the revision the board now carries. It moves the bus to 21.75 V, the
outlet target to 120.0 V RMS, and the transformer to 160 VA.

| Item | Rev B (17 V bus) | Rev C (21.75 V bus) | Why |
| --- | --- | --- | --- |
| `Rfbt1` | 49.9 kΩ | **64.9 kΩ** (`Rfbb1` stays 3.79 kΩ) | `1.2 × (1 + 64.9/3.79) = 21.749 V`; the LM5122 reference is 1.2 V |
| `U7` TPS62160 | on CSP (11–14 V) | **No change** | Netlist check: U7 pins 2/3 are on CSP, not the bus, so 21.75 V does not reach U7 in the present topology. No new buck is needed |
| `Q1` | 30 V candidate | **30 V `CSD17304Q3`**, applied | 21.75 V plus ringing; 30 V is the highest rating available in DQG0008A |
| `Q2` | 30 V | 30 V, unchanged | — |
| Transformer | `VPT24-4170`, 100 VA | **`VPT24-6670`, 160 VA** (8 %, 12 V @ 13.34 A) | 100 W needs about 111 VA through the transformer; a 100 VA part would run at 111 % of rating |
| `R27`/`R29`/`R31` | 68 kΩ | **68 kΩ**, applied | 25.74 V full scale covers the 21.75 V bus |
| Bus windows | ±0.75 V / ±1.5 V (as specified) | **±0.90 V / ±1.5 V**, applied | The start window was widened from the specified ±0.75 V because the tolerance band is ±0.63 V, which left only 0.12 V of slack |
| Compensation | 105 k / 28.7 k / 3.9 nF / 130 pF | **49.9 k / 80.6 k / 15 nF / 44.8 pF**, applied | Re-derived for the new operating point |
| `Cout1`–`Cout3` | 25 V 180 µF | **35 V 180 µF `35SVPK180M`**, applied | The 25 V parts ran at 89.5 % of rating at the top of the band |
| `Coutx1` | 47 µF 1206 | **22 µF 1206 35 V**, applied | A 47 µF 1206 at 35 V does not exist |
| `Rsense1` | 2 mΩ 1206 | **2 mΩ 2512 2 W**, applied | Value unchanged; the package moves for thermal headroom |
| `C13` | 22 µF tantalum | **as Rev B**, applied | The AMS1117 needs 22 µF with ESR |
| `U5`/`U6` bypass | 10 µF + 100 nF | **as Rev B**, applied | `CVDD ≥ 10 × CBOOT` with the 1 µF bootstrap capacitors |
| `J5` earth | cut the 3/4–GND tie | **cut, applied** — `J5` pins 3/4 are on their own `/PE` net | See item 7 above |

### What the 120 V target depends on

The board has **no isolated output sense**, so the firmware regulates an
estimate:

```
V_out_est = A_DROOP × V_drive_rms − B_DROOP × I_sec_est
A_DROOP = 8.8735   (the transformer's true no-load ratio, 115 / 12.96)
B_DROOP = 5.666 Ω  (the series impedance referred to the 115 V winding)
```

`A_DROOP` dominates. The earlier `115/12 = 9.583` was the **full-load** ratio,
and using it made the firmware believe the outlet was 120 V when it was really
about 111.5 V. Both constants are fitted per unit at bring-up: `A_DROOP` from
the no-load ratio measured with a DMM at the receptacle, `B_DROOP` from two
load points. With that fit the estimate is good to roughly 1 % across the load
range.

A direct isolated output sense (AMC1311B) is designed but **not in this
revision**. Until it is, the 120 V target is an estimate of an estimate, and
the calibration is not optional.

### Still open after Rev C

- The off-board output filter, the outlet fuse and MOV, and the input harness
  fuse, reverse-polarity protection and TVS are **harness items**, not board
  copper.
- The `J5` earth cut is on the board; the **harness** side of it is not. The
  enclosure class has to be chosen, and the earth either bonded to a chassis
  PE stud (Class I) or left unbonded with the unit marked double-insulated
  (Class II).
- The TMP235 placement and the `Q1`/`Q2` thermal vias are still open.
- A direct isolated output sense is designed but not in this revision.
- The 1.5 mm isolation rule and its 1.5005 mm closest approach are unchanged
  and re-verified by DRC against the Rev C layout. Nothing on the board comes
  closer than about 2.1 mm to any track, pad or via; the 1.5005 mm figure is
  to the pulled-back GND zone fill, where the rule bites.
- The board has still never been energised.

## Verification gate

Run in this order; record the result of each step.

1. **Before any edit:** export the netlist (`kicad-cli sch export netlist`) and
   save it. After each electrical change, export again and diff pad
   connectivity. Only the intended nets may change.
2. **ERC:** `kicad-cli sch erc --format json`. Baseline is 0 errors, 0 warnings.
   Treat any new `pin_not_driven` as a real change.
3. **DRC:** `kicad-cli pcb drc --schematic-parity`. Baseline is seven warnings
   (four dangling GND vias, one isolated `1V1` fragment, two J3 silkscreen
   clips) and 14 schematic-versus-footprint warnings (twelve `V` thermal-via
   pads on U4's footprint and two BOM-exclusion attributes on `J4`/`J6`). No
   new findings are acceptable.
4. **Zone refill:** refill all zones after any copper edit, or the plotted pour
   will not obey the 1.5 mm secondary rule.
5. **Barrier scan:** copper-to-copper clearance from `/BLACK` and `/RED` to every
   other net must be ≥ 1.5 mm. The closest approach is 1.5005 mm to the GND
   zone fill; any edit near those nets needs the scan re-run.
6. **Footprint/library:** `Rsense1` changes from 1206 to 2512 and `C13` from a
   0402 MLCC to a 3216 tantalum, so the board footprints and the library
   copies must agree. `kicad-cli pcb drc` must report no
   `lib_footprint_mismatch`.
7. **Bench, before the transformer:** measure the transformer leakage
   inductance and refer it to the secondary before adding it to the 38 mH filter
   inductance in the fc calculation. It is not in the repository and cannot be
   assumed. Verify the filter with a dummy load first; then connect the
   transformer.
8. **Bench, transformer:** apply 115.00 V RMS at 60 Hz to the 115 V winding and
   measure the open-circuit 12 V winding. That single measurement settles the
   turns ratio (about 12.96 V for the full-load convention) and therefore
   `A_DROOP`. Then load to 50 W and 100 W and record the outlet voltage and
   the 115 V winding current, which is what fits `B_DROOP`.
9. **Bench, gates:** scope GPIO1–4 at a modulation step with the gate drivers
   exposed. Confirm both inputs of each half-bridge are never high together and
   that the low-side pulse is shortened by 500 ns on both edges.
10. **First power-on:** dummy load before the transformer, current-limited
    supply, bridge disarmed. Confirm the 21.75 V bus, the AIN1 reading and the
    TMP235 readings, then enable the bridge at low modulation. Only then fit
    the output filter and the outlet.
11. **Do not fabricate a power-stage board without a fresh verification pass.
    Do not energise the power stage without an explicit human decision.**

## Values not verifiable from this repository

- Transformer leakage inductance and the actual regulation at 160 VA
  (catalogue says 8 % TYP, not measured, and there is no published limit).
- The transformer's actual no-load secondary voltage, which sets `A_DROOP`.
- Thermal resistance and junction temperature of Q1–Q6 in the actual enclosure.
- Whether the enclosure is Class I or Class II.
- Surge clamp of the chosen TVS at the actual test current; MOV leakage at the
  no-load condition.
- The measured loop cross-over and phase margin of the boost; the 9.3–11.8 kHz
  and ~80° figures are desk analysis.
- UCC27735 `CVDD ≥ 10 × CBOOT` against the exact datasheet revision.
- Actual output load, power factor and duty cycle at the craft fair.
