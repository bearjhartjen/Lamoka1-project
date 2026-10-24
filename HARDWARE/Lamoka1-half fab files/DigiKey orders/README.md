# DigiKey orders for the Lamoka1/2 build

What we ordered from DigiKey in October 2026, and what we left out. The quote
PDF below had our street address blacked out before committing; the order
details are untouched.

## Files

| Path | What it is |
| --- | --- |
| `Quote_19448680.pdf` | DigiKey quote 19448680 (22-Oct-2026, pricing valid through 22-Nov-2026): 67 lines covering the full assembly BOM. Street address redacted. |
| `digikey_cart_upload_quote19448680.csv` | The cart-upload CSV that produced the quote: part number and quantity per line. |
| `PCBWay_T-1N12W1065266A_components.xlsx` | The exact spreadsheet sent to PCBWay for order T-1N12W1065266A: manufacturer part numbers, quantities, unit prices, countries of origin and HS codes. It matches the DigiKey shipping invoice line for line, which is what customs requires. |

## The consigned assembly parts (shipped to PCBWay)

- Sales orders **102445691** (main box, 66 lines) and **102445690** (the L2
  Coilcraft inductor alone, a marketplace part in its own package), placed
  23-Oct-2026, **$237.10** total ($207.10 parts, $30.00 shipping).
- The 67 part numbers match the quote exactly.
- DigiKey ships both boxes direct to PCBWay's Hangzhou warehouse for kitted
  assembly under order T-1N12W1065266A. The inductor may arrive a few days
  after the main box.

## Final-assembly parts travel separately

The transformer, fan, outlet, thermal pad and shoulder washers are not
PCBWay assembly items and are not in the consigned boxes: the transformer
mounts off-board and wires to J1, the fan solders to the J4/J6 wire pads, and
the outlet wires to J5. They are ordered separately for local final assembly.

## What we omitted, and why

- **The transformer WAGO (J1, 2606-1108/010-000) is not in the DigiKey
  shipment.** It appears in neither the quote nor the consigned spreadsheet
  (only the J2 and J5 WAGOs are there). It was already the out-of-stock line
  on PCBWay's turnkey quotation. Source it separately before final assembly;
  without it the transformer has nowhere to land.
- **The transformer itself is not consigned to PCBWay either.** It is an
  off-board part wired to J1 at final assembly (see
  `HARDWARE/Transformer-wiring.md`), so it travels on the separate order
  above, not in the kitted boxes.

## Watch out before assembly starts

The package we sent PCBWay on 20-Oct has **112 placements**; the current
`HARDWARE/Lamoka1-half fab files/` package has **116**. The four new
placements are C34/C35/C36/C37, the bypass pair at each of the U5/U6 gate
driver VDD pins. The consigned DigiKey parts already include all four
(311-1344-1-ND covers C31/C35/C37, 445-5985-1-ND covers C34/C36/C24), but the
submitted gerbers have no pads for them. Send PCBWay the refreshed gerbers
and pick-and-place before assembly, or hand-fit the four caps.
