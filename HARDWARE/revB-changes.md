# Lamoka1/2 Rev B change set

> Status: **staged, not applied.** The main board has never been energised.
> Nothing in this file has been entered into the KiCad sources or the firmware.
> A Rev B board must not be fabricated, and the power stage must not be
> energised, without a fresh verification pass and an explicit human decision.
>
> Baseline: board `HARDWARE/Kicadfiles/Lamoka1-half/` as committed, firmware
> `SOFTWARE/Portal Interface Engine/V1.9.ino` targeting **105 V RMS at a 17 V
> bus**. Rev C (115 V) is a separate option at the end of this file and is not
> part of Rev B.

## Change table

| Reference | Current | Rev B | Reason |
| --- | --- | --- | --- |
| Off-board output filter, J5 → outlet | none | Series L 38 mH min, 1.5 A sat, ≥250 VAC insulation, <0.5 Ω DCR; shunt C 470 nF, ≥630 VDC/250 VAC polypropylene; series RC damper 100 Ω/1 W + 100 nF/630 VDC; bleeder 1 MΩ/1 W | 20 kHz PWM residue and switching edges at the outlet; fc ≈ 1.2 kHz, ≈ 49 dB at 20 kHz |
| Off-board output protection | none | 1.25 A/250 VAC slow-blow fuse in series with the outlet; 150 VAC MOV at the outlet | Output is unfused; 0.95 A at 105 V, 1.05 A at the ~96 V full-load outlet, so 1.25 A gives 1.19× at full load; verify the slow-blow carry |
| Input harness, J2 | 2-pole connector only | 15 A blade fuse; reverse-polarity protection; TVS; 12 AWG cable | 100 W at 11 V/0.85 = 10.7 A; no fuse or reverse protection today |
| `R27` | 43.2 kΩ 0603 `RC0603FR-0743K2L` | 68 kΩ 0603 `RC0603FR-0768KL` | AIN1 full scale 17.56 V → 25.74 V; 17 V bus sits at 2.18 V, not near the 3.3 V clip |
| `DIV_AIN1` (firmware) | `10/(43.2+10)` | `10/78`, then bus windows ±0.75 V start / ±1.5 V run | Matches the new divider; until then keep the ±0.4 V windows below the 17.56 V clip |
| `U5`/`U6` pin 7 (VDD) | no local bypass; VDD is CSP | 10 µF/25 V X7R + 100 nF at each pin to GND (or 1 µF + reduce CBOOT to 100 nF) | VDD is the 11–14 V CSP rail, not 17 V; gate-driver switching current has no local reservoir |
| `D11`/`D12` bootstrap diodes | SS14, anode CSP, cathode HB1/HB2 | no change; verify the same orientation on the board | Already correct in the netlist (D11/D12 pin 2 = A on CSP, pin 1 = K on HB1/HB2); record as verified, not redesigned |
| `C13` | 10 µF 0402 MLCC `CL05A106MQ5NUNC` | 22 µF tantalum `T491A226K010AT` (candidate) or 22 µF X5R + 1 Ω series | AMS1117 compensation expects 22 µF with ESR; near-zero-ESR MLCC is the risky condition |
| `Rsense1` | 2 mΩ 1206 `CSNL1206FT2L00` | 5 mΩ 2512 2 W `CSNL2512FT5L00` (candidate) | LM5122 IPEAK(CL) = 75 mV/RS: 37.5 A today, 15 A after; design peak is about 12.1 A |
| `J5` pins 3/4 | tied to board GND | Class II: leave outlet earth unbonded and mark it. Class I: cut the 3/4–GND tie and bond 3/4 to a PE stud | Board GND is not protective earth; the present tie makes every GND point an earth conductor |
| `U8`/`U9` (TMP235) | U8 in control section, U9 in inversion section | Keep U8; add thermal vias to Q1/Q2 or derate; consider moving U9 closer to Q4/Q6 | U9 is 10.3 mm from Q6 but 39.7 mm from Q5; U8 is 32–39 mm from the bridge. 85 °C trip is board temperature, not junction |
| `Q1` | `CSD16323Q3`, 25 V, DQG0008A | 30–40 V N-channel in DQG0008A; candidate `CSD17304Q3` (30 V, already used for Q2) | 25 V on a 17 V switching node leaves only 8 V for ringing; candidate must be verified for Rds(on), Qg and SOA |
| Transformer | `VPT24-4170`, 100 VA, 9 % regulation | No part change for 105 V; harness must follow `HARDWARE/Transformer-wiring.md` | 105 V/100 W keeps the 12 V drive winding at about 12.1 V RMS, within its 12 V rating. 115 V needs the higher bus, i.e. Rev C |

## Calculations

### 1. Output filter

- `fc = 1/(2π√(LC)) = 1/(2π√(38 mH × 470 nF)) = 1.19 kHz`.
- Attenuation at 20 kHz, second order: `40 log10(20/1.19) = 49.0 dB`.
- The 49 dB figure assumes an ideal inductor. Measure the 38 mH part's
  self-resonance and the transformer leakage; if the self-resonance is below
  about 200 kHz, the 20 kHz attenuation is set by parasitic capacitance, not
  the 38 mH. A segmented/gapped winding or a two-stage filter may be needed.
- Characteristic impedance `Z0 = √(L/C) = √(38 mH/470 nF) = 284 Ω`.
- Damping at 100 W: `R = 105²/100 = 110 Ω`, `ζ = Z0/(2R) = 1.29`.
  This is at or above critical damping; `ζ ≈ 1.0` near 80 W. There is no
  resonant peaking at full load.
- Damper corner: `1/(2π × 100 Ω × 100 nF) = 15.9 kHz`; the 100 nF blocks DC, so
  the 100 Ω does not dissipate at 60 Hz.
- Bleeder: `τ = 1 MΩ × 470 nF = 0.47 s`; 163 V decays to below 50 V in about
  0.56 s.
- Inductor DC loss at 0.95 A: `0.95² × 0.5 Ω = 0.45 W`; at the full-load
  1.05 A it is 0.55 W. The 1 W bleeder sees `163²/1 MΩ = 0.027 W`.
- The 470 nF shunt is an across-the-line capacitor. A 250 VAC polypropylene
  part is adequate at 105–115 V, but an X2 (275 VAC) part is preferred for
  agency approval. The 630 VDC rating covers the 163 V no-load peak with margin.

### 2. Input harness

- Worst-case input current: `100 W/(11 V × 0.85) = 10.7 A`; 15 A/10.7 A = 1.40.
- 12 AWG has roughly 20 A bundled / 30 A free-air ampacity; verify against the
  chosen insulation and bundle. At 10.7 A the drop is small.
- Reverse-polarity P-channel MOSFET: ≥30 V, ≤10 mΩ at the available gate drive,
  with a 15 V gate-source clamp. At 10 mΩ the loss is `10.7² × 0.01 = 1.15 W`.
- Schottky alternative: at 0.5 V forward, `10.7 × 0.5 = 5.4 W`, so it needs a
  heatsink; the MOSFET is preferred.
- TVS: choose a standoff above the 14 V maximum and a clamp below Q1's rating.
  A 15 V standoff part clamps around 24 V; with Q1 at 25 V the margin is thin,
  which is a second reason to move Q1 to 30 V. Verify the surge clamp at the
  actual test current, not just the datasheet headline.

### 3. AIN1 divider

- `DIV_AIN1 = 10/(68 + 10) = 0.128205`.
- Full scale: `3.3/0.128205 = 25.74 V`; 17 V reads 2.18 V, 19.4 V reads 2.49 V.
- Divider current: `3.3 V/78 kΩ = 42 µA`; R27 dissipates
  `(25.74 − 3.3)²/68 kΩ = 7.4 mW`.
- Current divider clips at `3.3 × 53.2/10 = 17.56 V`. The existing ±0.4 V
  windows (16.6–17.4 V) stay below that, which is why they must remain until
  R27 and `DIV_AIN1` change together.
- After the change, restore start ±0.75 V and run ±1.5 V. At 17 V nominal the
  run window is 15.5–18.5 V; confirm this does not trip during boost start-up.

### 4. Gate-driver bypass

- VDD is `/Inversion-Section/CSP`, 11–14 V, confirmed in the netlist for U5
  pin 7, U6 pin 7, U5/U6 pin 4 and the D11/D12 anodes. The 17 V bus is not
  the driver supply.
- Datasheet rule: `CVDD ≥ 10 × CBOOT`. C22/C23 are 1 µF bootstrap capacitors,
  so the strict value is 10 µF, not 1 µF. Either raise CVDD to 10 µF or reduce
  CBOOT to 100 nF; verify against the UCC27735 datasheet before laying out the
  bypass. The proposed 1 µF + 100 nF is the typical recommendation for a
  100 nF bootstrap capacitor and is **not** sufficient as written for 1 µF.
- Place the 100 nF within a few millimetres of pin 7 and the 1 µF (or 10 µF)
  next to it, with a short return to the pin 5 GND.

### 5. C13 / AMS1117-3.3 output

- AMS1117 requires about 22 µF on the output, and the ESR is part of its
  compensation. A 10 µF 0402 MLCC has essentially zero ESR and is the unstable
  condition.
- `T491A226K010AT` is 22 µF, 10 V, ±10 %, A case (candidate; verify ESR and
  derating). At 3.3 V the 10 V rating gives 3× margin.
- Alternative: 22 µF X5R 1206 in series with 1 Ω, which gives a defined ESR
  without a polarized part. Do not fit a bare 22 µF MLCC.

### 6. Rsense1

- `IPEAK(CL) = 75 mV/RS`. At 2 mΩ: 37.5 A. At 5 mΩ: 15.0 A.
- Design peak is about 12.1 A, so 15 A gives about 24 % margin and keeps the
  inductor below its saturation class. The 2 mΩ part allowed the inductor to
  saturate before the limit tripped.
- Shunt power at 12 A: `12² × 0.005 = 0.72 W`; a 2 W 2512 part runs at 36 %.
  At the 15 A limit it is 1.125 W (56 %).
- This is a footprint change (1206 → 2512). Keep Kelvin sense connections and
  re-derive the LM5122 slope/compensation: Rslope1 is 105 kΩ for the 2 mΩ
  shunt, and the current-sense signal scales with RS. Verify COMP and the
  current-limit behaviour on the bench before connecting the transformer.

### 7. J5 earth

- Netlist: J5 pins 3/4 are on `GND`; J5 pins 1/2 are `/BLACK` and 5/6 are
  `/RED`. Board GND is therefore the outlet earth today.
- Class II (double-insulated enclosure): leave the outlet earth pin unbonded,
  mark the outlet and the enclosure, and do not rely on board GND for
  protection. The 1.5 mm secondary barrier remains the only separation.
- Class I: cut the J5 3/4-to-GND tie, bond J5 3/4 to a dedicated PE stud with a
  low-inductance connection, and keep the 1.5 mm barrier. Never use board GND
  as the PE conductor.
- The enclosure class is a mechanical decision not recorded in the repository;
  choose it before the harness is built.

### 8. Thermal monitoring

- Measured placement: U9 is 10.3 mm from Q6, 29.1 mm from Q3, 39.7 mm from Q5;
  U8 is 32.1 mm from Q6 and 32.8 mm from Q3.
- The TMP235s report board temperature. The 85 °C trip therefore protects the
  board, not the FET junction; a TO-220 or DQG0008A junction can be much hotter
  than the board at the same instant.
- Q1/Q2 are DQG0008A with **zero thermal vias** (checked in the board file).
  Q3–Q6 are TO-220-3 vertical with no heatsink.
- Rev B options: add a thermal via array under Q1/Q2 pads 9 (and connect to the
  GND/copper pour), or derate the continuous output. Consider moving U9 closer
  to Q4/Q6, the two devices farthest from it. Record the resulting junction
  temperature budget; it is not in the repository today.

### 9. Q1 voltage margin

- Q1 is `CSD16323Q3`, 25 V, on a 17 V switching node. That leaves 8 V for
  leakage-inductance ringing at ~495 kHz with no snubber.
- Q2 is already `CSD17304Q3`, 30 V, in the same DQG0008A footprint. Using the
  same 30 V part for Q1 is the lowest-risk change; a 40 V part in the same
  footprint would be better if one is available. Candidate only: verify
  Rds(on) at the LM5122 gate drive, Qg, SOA and availability before ordering.
- Raising Q1 to 30 V also improves the input TVS clamp margin (see item 2).

### 10. Transformer

- `VPT24-4170`: 100 VA, 9 % regulation, 45 °C rise, eight leads. The board
  drives the 12 V winding and takes the outlet from the 115 V winding; the lead
  map is `HARDWARE/Transformer-wiring.md` and must not be fitted colour-for-
  colour.
- At 105 V/100 W the drive winding carries `105/(115/12) = 10.96 V RMS` at full
  load, about 12.1 V allowing for regulation and losses. That is within the
  12 V winding rating, so 105 V is a valid Rev B target on the 17 V bus.
- The 115 V winding sees about 105 V at the outlet, well inside the 1.5 mm
  isolation rule and the 163 V no-load ceiling.
- Load-regulation note: the firmware infers the outlet from the drive voltage
  using the 115:12 ratio and regulates that inferred value to 105 V. The
  transformer's 9 % regulation means the actual outlet is about 105 V at no
  load and about 96 V at 100 W (105 × 0.91). At `PWM_M_MAX` the no-load outlet
  is about 110.6 V and the full-load outlet is about 100.6 V. If a flat 105 V
  at full load is required, the firmware would need a direct outlet sense or a
  load correction (neither is present), or the Rev C 19.4 V bus. This is a
  firmware/verification item, not a Rev B hardware change.

## Rev C option (115 V)

**Not part of Rev B. Not applied. Do not mix these changes into a Rev B build.**

115 V at the outlet needs about a 19.4 V bus. That is a new operating point, not
a tweak, because:

| Item | Rev B (17 V bus) | Rev C (≈19.4 V bus) | Why |
| --- | --- | --- | --- |
| `Rfbt1` | 49.9 kΩ | 57.6 kΩ (Rfbb1 stays 3.79 kΩ) | `1.2 × (1 + 57.6/3.79) = 19.44 V`; the LM5122 reference is 1.2 V |
| `U7` TPS62160 | on CSP (11–14 V), 17 V input max | No change if U7 stays on CSP. If Rev C re-derives the 3.3 V rail from the 19.4 V bus, use a 24–28 V buck (candidate: TPS54331, LMR33630) | Netlist check: U7 pins 2/3 are on CSP, not the 17 V bus, so 19.4 V does not reach U7 in the present topology. The 17 V limit is a blocker only for a bus-fed 3.3 V rail |
| `Q1` | 30 V candidate | 30–40 V, review Q2 as well | 19.4 V plus ringing; both switches sit on the higher node |
| Transformer | VPT24-4170, 100 VA | Larger VA or accepted derating | 115 V/100 W draws about 112 VA and raises the 12 V winding to 13.2 V RMS (9.75 % over 12 V, flux ≈ 1.65 T) |
| `R27` | 68 kΩ | 68 kΩ (same) | 25.74 V full scale still covers 19.4 V |
| Bus windows | ±0.75/±1.5 V | ±0.75/±1.5 V | Same divider and firmware after the Rev B change |

Rev C consequences in detail:

- The TPS62160 is fed from CSP, not the 17 V bus (U7 pins 2/3 in the netlist).
  If Rev C keeps that architecture, the buck does not see 19.4 V and does not
  need replacing. If the 3.3 V rail is re-derived from the 19.4 V bus, U7 must
  become a 24–28 V part. Resolve which architecture Rev C uses before ordering.
- The 112 VA load overrates the 100 VA transformer. At 13.2 V RMS on the 12 V
  winding the flux rises from about 1.5 T to 1.65 T, reducing saturation margin
  and increasing core loss. A 150 VA transformer, or a documented 100 W derating,
  is required; neither is selected here.
- The 19.4 V bus is 78 % of the 25 V polymer output capacitors' rating. Check
  ripple and transient overshoot before accepting it; a 35 V capacitor family
  would give more margin.
- U5/U6 VDD remains CSP (11–14 V), so the gate-driver bypass change is unchanged.
- The output filter, fuse, MOV and bleeder from Rev B remain valid at 115 V:
  1.25 A gives 1.44× margin at 0.87 A, and the 630 VDC/250 VAC parts cover
  the 163 V peak. The 150 VAC MOV sees the filtered 115 VAC fundamental, not
  the raw 163 V square wave; without the filter it would be marginal. Verify
  the carrier residue, leakage and thermal margin.
- Rev C still needs the AIN1 68 kΩ divider. Without it, 19.4 V would drive AIN1
  to 2.49 V only after the change; with the old 43.2 kΩ divider it would clip.
- Rev C needs a fresh transformer VA/flux calculation and a new isolation review.
  Do not assume the Rev B 1.5 mm barrier result carries over unchanged.

## Verification gate

Run in this order; record the result of each step.

1. **Before any edit:** export the netlist (`kicad-cli sch export netlist`) and
   save it. After each electrical change, export again and diff pad
   connectivity. Only the intended nets may change.
2. **ERC:** `kicad-cli sch erc --format json`. Baseline is 0 errors, 0 warnings.
   Treat any new `pin_not_driven` as a real change.
3. **DRC:** `kicad-cli pcb drc --schematic-parity`. Baseline is seven warnings
   (four dangling GND vias, one isolated `1V1` fragment, two J3 silkscreen
   clips) and 17 symbol-versus-footprint warnings. No new findings are acceptable.
4. **Zone refill:** refill all zones after any copper edit, or the plotted pour
   will not obey the 1.5 mm secondary rule.
5. **Barrier scan:** copper-to-copper clearance from `/BLACK` and `/RED` to every
   other net must be ≥ 1.5 mm. The closest approach was 1.5005 mm; any edit near
   those nets needs the scan re-run.
6. **Footprint/library:** `Rsense1` changes from 1206 to 2512, so the board
   footprint and the local library copy must agree. `kicad-cli pcb drc` must
   report no `lib_footprint_mismatch`.
7. **Bench, before the transformer:** measure the transformer leakage inductance
   and refer it to the secondary before adding it to the 38 mH filter
   inductance in the fc calculation. It is not in the repository and cannot be
   assumed. Verify the filter with a dummy load first; then connect the
   transformer.
8. **Bench, gates:** scope GPIO1–4 at a modulation step with the gate drivers
   exposed. Confirm both inputs of each half-bridge are never high together and
   that the low-side pulse is shortened by 500 ns on both edges.
9. **First power-on:** dummy load before the transformer, current-limited supply,
   bridge disarmed. Confirm 17 V bus, AIN1 reading, TMP235 readings, then enable
   the bridge at low modulation. Only then fit the output filter and the outlet.
10. **Do not fabricate a power-stage board without a fresh verification pass.
    Do not energise the power stage without an explicit human decision.**

## Values not verifiable from this repository

- Transformer leakage inductance and the actual regulation at 100 VA (catalogue
  says 9 %, not measured).
- Thermal resistance and junction temperature of Q1–Q6 in the actual enclosure.
- Whether the enclosure is Class I or Class II.
- Surge clamp of the chosen TVS at the actual test current; MOV leakage at the
  163 V no-load condition.
- Availability, Rds(on), Qg and SOA of the Q1 and input-MOSFET candidates.
- UCC27735 `CVDD ≥ 10 × CBOOT` against the exact datasheet revision; the 1 µF
  bootstrap capacitors make the proposed 1 µF bypass marginal.
- TPS62160 absolute-maximum input voltage, and whether Rev C keeps the 3.3 V
  rail on CSP or re-derives it from the 19.4 V bus.
- Actual output load, power factor and duty cycle at the craft fair.
