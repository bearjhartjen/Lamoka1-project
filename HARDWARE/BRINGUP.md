# Bringing a Lamoka up

This is the whole process, in the order we do it, in plain language. Two
boards are covered:

- the **Lamoka1-DEV-Brain** — the control board: microcontroller, one button,
  ten LEDs. USB-C only, no mains anywhere. This is where the firmware is
  developed, and it is a product in its own right.
- the **Lamoka1/2** — the inverter: battery in, 120 V RMS AC out.

We do the DEV-Brain first because it is the safe one, and because getting the
firmware onto a real board first means the power board gets the firmware you
already trust.

> [!WARNING]
> The Lamoka1/2 output is **120 V RMS AC** — the same as a wall outlet. It is
> live whenever it is switched on. Work through the stages in order, do not
> skip ahead, and never leave a powered unit unattended.

- [Stage 0 — Before you start](#stage-0--before-you-start)
- [Stage 1 — The DEV-Brain (safe, USB only)](#stage-1--the-dev-brain-safe-usb-only)
- [Stage 2 — The Lamoka1/2, low voltage first](#stage-2--the-lamoka12-low-voltage-first)
- [Stage 3 — The outlet](#stage-3--the-outlet)
- [You are done when](#you-are-done-when)
- [What this prototype does not have](#what-this-prototype-does-not-have)
- [If something goes wrong](#if-something-goes-wrong)
- [Where to go next](#where-to-go-next)

## Stage 0 — Before you start

You need:

| | |
| --- | --- |
| The assembled board | Lamoka1-DEV-Brain and/or Lamoka1/2 |
| A multimeter | A DMM is good enough; a clamp meter helps on the 115 V side |
| A USB-C cable | For the DEV-Brain, and for the low-voltage check on the Lamoka1/2 |
| A current-limited bench supply | Or a 12 V battery in a safe fixture, with a way to cut it |
| A resistive load | For the outlet test — see [Stage 3](#stage-3--the-outlet) |
| A scope | For the gate-drive check on the Lamoka1/2. If you do not have one, get a borrow or a cheap one — this is the one step we do not want to skip |

The Lamoka1/2 is a 2-layer FR-4 board, 95.2 × 95.2 mm, with SMD parts on both
sides. If you are hand-assembling it, plan a two-pass reflow (or a stencil and
a hot plate), then hand-solder the through-hole parts: the Wago terminals, the
button, the USB-C shield, and the TO-220 devices.

## Stage 1 — The DEV-Brain (safe, USB only)

### 1.1 Plug it in

Plug the USB-C cable into the DEV-Brain and into your computer. Nothing else is
connected. There is no mains and no battery involved.

**You should see:** the LEDs do something on power-up.

### 1.2 Put the firmware on it

The DEV-Brain runs **PIE V1.8**, the bench-tested head. It has been running
for 72 hours straight on our bench without trouble.

**With the Arduino IDE:**

1. **File → Preferences → Additional Boards Manager URLs**, and add:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. **Tools → Manage Boards**, search for and install **Raspberry Pi
   Pico/RP2040/RP2350**.
3. **Tools → Manage Libraries**, install **FastLED**.
4. **File → Open**, and open `SOFTWARE/Portal Interface Engine/V1.8.ino`.
5. **Tools → Board**, pick **Raspberry Pi RP2350**.
6. **Tools → Port**, pick the port that appeared when you plugged the board in.
7. Click **Upload**.

**With the command line** (`arduino-cli`):

```bash
arduino-cli core install rp2040:rp2040 \
  --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli lib install FastLED

make firmware                    # compile the DEV-Brain head (V1.8)
make upload PORT=/dev/ttyACM0    # compile and flash it
```

**If the port does not appear:** the RP2350 can also come up in USB bootloader
mode on its own. In the board's own UI, go to **Settings → BOOTSEL**. The
DEV-Brain has no BOOTSEL button of its own — you pick it from the menu. The
port then reappears as a new device, and you upload again.

**You should see:** the status ball and the Fireball game respond to the
button. Tap to move, double-tap to go back, hold to select.

### 1.3 Stop here for a while

The DEV-Brain is where the control side is developed, and it is the board we
develop the lights and the button on. If you are building the Lamoka1/2, you
want V1.8 working here first — it is the same code that runs the interface on
the main board.

Ordering your own DEV-Brain, and the step-by-step ordering and flashing
instructions, are in the
[Lamoka1-DEV-Brain guide](../Lamoka1-DEV-Brain/Readme.md).

## Stage 2 — The Lamoka1/2, low voltage first

> [!CAUTION]
> From here on the board can make 120 V RMS AC. Nothing after the transformer
> is measured — there is no voltage or current sensing on the 115 V side at
> all. The firmware regulates a computed estimate of the outlet, and it cannot
> see a fault there. From here on, **measure the output yourself with a meter**
> and treat everything past the transformer as live mains.

### 2.1 Look at it

With the power off, check the board over:

- The electrolytic cans (`Cin1`, `Cout1`–`Cout3`) and the tantalum (`C13`) are
  the right way round.
- No solder bridges, especially around the fine-pitch parts and the USB-C
  receptacle.
- Continuity: **no short** between `+BATT` and `GND`, and none between the
  21.75 V bus and `GND`.

**If there is a short:** do not power it. Find it first. A short across the
input or the bus is the one fault that will destroy the board immediately.

### 2.2 USB-C only (5 V)

Plug in USB-C. Nothing else connected.

On USB alone, the 5 V rail comes up through the OR-ing diodes (`CR1`/`CR2`)
and the buck converter stays off.

**You should see:** 5 V on the 5 V rail, and then 3.3 V on the 3.3 V rail and
1.1 V on the RP2354A's core rail.

**If you see nothing:** check the 5 V rail first, then the 3.3 V rail, then
the 1.1 V rail, in that order.

### 2.3 Battery or bench supply

Now connect the input: 11–14 V DC on `+BATT` and `GND`. Use a current-limited
bench supply starting at about **0.5 A**, or a 12 V battery in a fixture where
you can cut it instantly.

**You should see:** the boost converter comes up and the **21.75 V bus**
appears. That bus is set by the LM5122's 1.2 V feedback reference with
`Rfbt1` = 64.9 kΩ and `Rfbb1` = 3.79 kΩ, giving 21.749 V. The input current
settles at whatever the load needs.

**If the input current runs away:** disconnect immediately. That is a fault in
the boost stage, not something to work around.

### 2.4 Gate-drive check

Transformer still disconnected. This is the one check we do not skip.

Put a scope on the four gate-driver inputs (GPIO1–GPIO4 leave the MCU and go
to `U5`/`U6`), and watch them with the bridge armed.

**You should see:**

- The two inputs of each half-bridge **never high at the same time**. The
  UCC27735 has interlock, but it has no built-in dead time — the dead time is
  entirely the firmware's job.
- The low-side pulse shortened by the **500 ns** gap on both edges.
- A 20 kHz carrier with a 60 Hz envelope.

**If the two inputs of a leg overlap:** stop. Do not connect the transformer.

### 2.5 The earth

Before the outlet goes anywhere, sort out the earth.

`J5` pins 3/4 are the outlet's earth pole, and they sit on their own `/PE`
net — **not** the board's logic ground. Board `GND` is not protective earth.

- **Metal enclosure (Class I):** bond `J5` 3/4 to a chassis PE stud (M3).
- **Plastic enclosure (Class II):** leave it unbonded and mark the unit
  double-insulated.

A three-prong outlet must never ship with its earth floating or tied to the
board's logic ground.

## Stage 3 — The outlet

### 3.1 Wire the transformer

Read [`HARDWARE/Transformer-wiring.md`](Transformer-wiring.md) first — all of
it. The short version:

> **Do not match colours.** The `J1` net names are lead colours, and the
> transformer's leads do **not** go on colour-for-colour. The `/BLUE` net
> carries the BLACK lead. Fitted colour-for-colour, the inverter makes about
> 1.9 V instead of 120 V.

| J1 pole | Net | Lead |
| --- | --- | --- |
| 1, 2 | `/BLUE` | BLACK, ORANGE |
| 3, 4 | `/BROWN` | RED, YELLOW |
| 5, 6 | `/BLACK` | BLUE, VIOLET |
| 7, 8 | `/RED` | BROWN, GRAY |

Each net takes two poles, and the two-poles-per-net block *is* the parallel
connection the transformer wants. No jumpers on the transformer.

### 3.2 First output test

Start with a modest load — a 100 W, 120 V incandescent or resistive heater is
about 1.36 A at the outlet.

Put your DMM on the outlet, and turn the inverter on.

**You should see:** about **120 V RMS**.

Here is the honest version of what that number means. The board has no output
sensing, so the firmware regulates an estimate built from the transformer's
catalogue numbers. Those numbers hold the real outlet within about **3 % of
120.0 V RMS** across the load range for a transformer regulating 6–10 % (the
catalogue value is 8 %), and **no unit needs to be calibrated or fitted**.

So a good unit reads somewhere in **116–124 V RMS**, and a unit at the edge of
the expected transformer spread still lands inside that. If your unit reads
outside it, stop and check the transformer wiring and the boost bus — the
constants are not the thing to change.

**Measure it. Do not assume it.** The estimate is good, but it is an estimate,
and this is the whole reason the caution box above exists.

### 3.3 Under load

Try the outlet at a couple of loads and watch it again:

| Load | Outlet | Input current |
| --- | --- | --- |
| Nothing | ~120 V RMS | A few tens of mA |
| 50 W | ~120 V RMS | ~0.7 A at 12 V |
| 100 W | ~120 V RMS | ~1.4 A at 12 V |

The product rating is **100 W continuous**. The transformer is 160 VA, so
there is headroom, but the rating is 100 W.

## You are done when

- [ ] The DEV-Brain boots, the LEDs run, and the button works.
- [ ] The Lamoka1/2 has no shorts, comes up on USB-C alone, and the 3.3 V and
      1.1 V rails are right.
- [ ] The 21.75 V boost bus comes up from a current-limited supply, and the
      input current is sensible.
- [ ] The gate-drive check shows no overlap between the inputs of a leg, and
      the 500 ns dead time on both edges.
- [ ] The transformer is wired per the table, and you checked it twice.
- [ ] The outlet reads about 120 V RMS, measured with a meter, under a light
      load and under a full load.
- [ ] The earth is bonded to a chassis PE stud, or the unit is marked
      double-insulated.

## What this prototype does not have

Straightforward, so there are no surprises later:

- **No charger.** That comes in a later Lamoka1 model.
- **No voltage or current measurement after the transformer.** This is the one
  that matters — see the caution boxes above.
- **The temperature sensors read board temperature**, not FET junction
  temperature.

## If something goes wrong

| Symptom | Look here |
| --- | --- |
| No 5 V from USB | The USB-C receptacle's solder joints; the CR1/CR2 OR-ing diodes |
| 5 V but no 3.3 V | The buck converter (U7) and its inductor; the AMS1117 (U2) and `C13` |
| No 3.3 V, no 1.1 V | The RP2354A supply path |
| The boost bus does not come up | The LM5122 (U4), `Rfbt1`/`Rfbb1`, and the `CSS1` soft-start capacitor |
| The input current runs away | Disconnect and stop. That is the boost stage, not a firmware problem |
| The bridge outputs nothing | The arming gesture in the UI; the MCU's GPIO1–4; the gate drivers' VDD |
| Both inputs of a leg go high | Stop. Do not connect the transformer |
| The outlet reads out of range | The transformer wiring first (colour-for-colour is the classic mistake), then the boost bus |
| Nothing happens when you press the button | Is the board in BOOTSEL mode? Is the right firmware head flashed? |

## Where to go next

- [`HARDWARE/Transformer-wiring.md`](Transformer-wiring.md) — the full `J1`
  wiring map and the hazards.
- [`HARDWARE/COMPONENTS.md`](COMPONENTS.md) — what every part is, in plain
  English.
- [`HARDWARE/Design_Notes.md`](Design_Notes.md) — why it is built this way.
- [`SOFTWARE/Portal Interface Engine/PIE-v1.9-information.md`](../SOFTWARE/Portal%20Interface%20Engine/PIE-v1.9-information.md)
  — what the main-board firmware does.
- [`SOFTWARE/Portal Interface Engine/PIE-v1.8-information.md`](../SOFTWARE/Portal%20Interface%20Engine/PIE-v1.8-information.md)
  — what the DEV-Brain firmware does.
- [`../README.md`](../README.md) — the rest of the project.
