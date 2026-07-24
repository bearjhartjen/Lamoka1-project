# Lamoka1 charging section — manual work still required

The automatic patch changes safe text fields and standard low-power footprints.
It deliberately does not guess high-current manufacturer land patterns or rewrite
power-stage topology.

## Required before PCB layout

1. Replace Q6–Q9 generic three-pin NMOS symbols with verified
   `SIR680LDP-T1-RE3` PowerPAK SO-8 symbols and footprints.
   Verify physical mapping: pins 1–3 source, pin 4 gate, pins 5–8 plus exposed
   pad drain.

2. Import and verify the manufacturer land pattern for
   `SRP1510CA-100M` at L1.

3. Select exact purchasable high-power shunts for R12 = 2 mOhm and
   R14 = 5 mOhm, then use their exact footprints. Route ACP/ACN and
   SRP/SRN as true Kelvin pairs.

4. Select exact 63 V low-ESR parts and footprints for C15, C20 and C21.

5. Add the safe-default MCU-controlled 4S/8S feedback switch:
   - R24 = 249 kOhm
   - R25 = 29.4 kOhm, default 4S
   - add R32 = 26.1 kOhm in parallel with R25
   - switch R32 with a low-leakage device controlled by `CHG_8S_SELECT`
   - add a 100 kOhm pulldown so loss of MCU power defaults to 4S
   - firmware must disable CE before changing profile

6. Expose/connect top-level nets:
   `I2C_SCL`, `I2C_SDA`, `CHG_CE`, `CHG_INT`, `3v3`,
   `VIN-SOURCE`, and `CHG_8S_SELECT`.

7. STAT1, STAT2 and PG may remain NC because CHG_INT plus I2C status
   registers provide the MCU with charger state. Connect them only if separate
   hardwired status GPIOs are required.

8. First power-up must use a current-limited bench supply, no battery,
   thermal monitoring, and oscilloscope checks of gate drive and SW1/SW2.
