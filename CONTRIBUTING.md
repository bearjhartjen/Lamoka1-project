# Contributing to the Lamoka1 project

Thanks for your interest in the Lamoka1! This is an open-source inverter/charger
project, and help is welcome in every area: hardware review, firmware,
documentation, testing, and mechanical design.

This guide covers the development setup, the conventions used in this repository,
how to test your changes, and how to get a change merged.

## Contents

- [Ways to contribute](#ways-to-contribute)
- [Before you start](#before-you-start)
- [Setup](#setup)
- [Code style](#code-style)
- [Testing](#testing)
- [Submitting changes](#submitting-changes)
- [Reporting bugs and requesting features](#reporting-bugs-and-requesting-features)
- [Community standards](#community-standards)
- [Contact](#contact)

## Ways to contribute

- **Hardware** — review the schematic and layout, reproduce the design,
  suggest component or layout improvements, or verify calculations against
  datasheets. The main board lives in `HARDWARE/Kicadfiles/Lamoka1-half/`.
- **Firmware** — the Portal Interface Engine (PIE) is in
  `SOFTWARE/Portal Interface Engine/`. Anything that makes the UI better, or
  that helps bring up the power stage, is valuable.
- **Documentation** — design notes, component descriptions, build
  instructions. If something confused you, it will confuse the next person too;
  a fix is a real contribution.
- **Testing** — a second pair of eyes on a bench setup, a successful build, or
  a firmware test on real hardware is always useful.
- **Mechanical** — enclosure models, mounting ideas, and STEP/STL exports that
  make the hardware easier to integrate.

## Before you start

- **Read the design documents first.** [`HARDWARE/Design_Notes.md`](HARDWARE/Design_Notes.md),
  [`HARDWARE/COMPONENTS.md`](HARDWARE/COMPONENTS.md), and the per-board READMEs
  explain the architecture and why things are the way they are.
- **The KiCad sources are the specification.** Prose documentation can lag
  behind the design; when they disagree, the design files win, and the prose
  should be corrected in the same change.
- **AI is used as a tool, not an authority.** See the "Usage of AI in this
  repository" section of the [README](README.md#usage-of-ai-in-this-repository).
  Any contribution — human or AI-assisted — must be reviewed and understood by
  the person submitting it before it lands.
- **Safety first.** The Lamoka1/2 produces mains voltage. Do not energize the
  power stage of a board you built until you have completed the staged bring-up
  in the [README](README.md#build-and-fabrication), and never work on live
  mains wiring alone.

## Setup

### Repository tooling (Python checks)

The repository checks and the ruff linter run on Python 3 through a pinned
[uv](https://docs.astral.sh/uv/) environment defined by `pyproject.toml` and
`uv.lock`:

```bash
git clone https://github.com/bearjhartjen/Lamoka1-project.git
cd Lamoka1-project
uv sync                 # create .venv/ from the lockfile (one time)
make test               # repository checks (pytest)
make lint               # ruff check and format check
```

If you would rather not use uv, `python3 -m pip install pytest ruff` gives the
same tools; `make test` and `make lint` work either way.

### Hardware (KiCad)

The projects are **KiCad 10** projects. Symbols and footprints are embedded in
the `.kicad_sch` and `.kicad_pcb` files, so you do **not** need external
libraries to open them — just install KiCad and open the project:

```bash
kicad "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pro"          # main board
kicad "Lamoka1-DEV-Brain/Kicad files/Lamoka1-DEV-board.kicad_pro"  # DEV-Brain
```

`kicad-cli` (bundled with KiCad) is used for command-line checks, netlist
comparison, and fabrication exports. 3D models come from KiCad's standard
`KICAD10_3DMODEL_DIR`; expect missing component bodies in the 3D viewer only if
your KiCad installation lacks the standard model library.

> [!IMPORTANT]
> If KiCad is open on a project, a `.lck` file exists in the project directory.
> Don't edit those files from the command line while the GUI holds them, or the
> GUI can silently overwrite your work. Close KiCad, or work on a copy.

### Firmware (arduino-pico)

The firmware is built with the [arduino-pico](https://github.com/earlephilhower/arduino-pico)
core (RP2040/RP2350) and [FastLED](https://github.com/FastLED/FastLED):

1. In the Arduino IDE 2.x, add the board manager URL
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
   under **File → Preferences → Additional Boards Manager URLs**, then install
   the **Raspberry Pi Pico/RP2040/RP2350** core.
2. Install **FastLED** from the Library Manager.
3. Open the sketch you want to work on under `SOFTWARE/Portal Interface Engine/`.

Or with `arduino-cli`, plus the repository `Makefile` targets that wrap it:

```bash
arduino-cli core install rp2040:rp2040 \
  --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli lib install FastLED

make firmware                     # compile the default sketch (DEV-Brain head, V1.8)
make upload PORT=/dev/ttyACM0     # compile and flash
make firmware SKETCH="SOFTWARE/Portal Interface Engine/V1.9.ino"   # the main-board head
```

Copy [`.env.example`](.env.example) to `.env` to keep local values such as
`PORT`, `SKETCH`, or `FQBN` without typing them on every command; `.env` is
gitignored.

Arduino compiles every `.ino` in a sketch folder together and requires the
folder and main file to share a name, so the PIE revisions cannot be compiled
straight out of `SOFTWARE/Portal Interface Engine/`. `make firmware` copies the
chosen revision into its own folder under `build/` and compiles it there with
warnings enabled (`-Wall -Wextra`), so no revision needs to be renamed or moved
by hand.

### Documentation and tooling

The docs are plain Markdown. The Python checks environment (above) provides
pytest and ruff; KiCad's `kicad-cli` covers fabrication exports and checks, and
a Python install with KiCad's `pcbnew` module is only needed if you script the
KiCad Python API.

### Board images

The board pictures in `Images/` are generated, not hand-drawn, so a layout
change that shows up in a picture does not leave the READMEs showing a board
that no longer exists. `make images` (`tools/make_images.sh`) regenerates all of
them: the three Lamoka1/2 views and both DEV-Brain views come from
`kicad-cli pcb render`, and the routing view is a 2D plot of the F.Cu and B.Cu
layers. KiCad 10 and ImageMagick are both required — ImageMagick trims the
transparent border off each render and rasterises the routing plot.

The two DEV-Brain views additionally need that board's 3D models, which are not
committed; link the local model folder at
`Lamoka1-DEV-Brain/Kicad files/components` to include them. Without it the
script still regenerates the Lamoka1/2 images and skips the DEV-Brain pair with
a message. Commit the regenerated images together with the change that altered
them.

## Code style

### KiCad projects

- Use KiCad 10 and save in the current file format; don't downgrade file
  versions.
- Keep symbols and footprints embedded in the schematic/board files. If you add
  a project-local part, put it in `Lamoka1-half_Parts.kicad_sym` or the appropriate
  project library on your machine: library sources are kept local and are not
  committed (see `AGENTS.md`, "Repository practice").
- Fill in the `MPN` field on every component, and `Manufacturer` where known —
  the BOM export depends on them.
- **Do not rename nets casually.** Net names are part of the schematic's
  documentation and are used by the firmware documentation; a rename that is
  not intentional shows up as a netlist diff in review.
- Keep zone fills, DRC severities, and netclasses consistent with the project
  settings; don't commit editor state (`*.kicad_prl`) or backup folders — the
  `.gitignore` files already exclude them.
- Fabrication output (Gerbers, drill files, position files) is committed for
  released revisions so boards can be reproduced. Keep it out of commits for
  work-in-progress changes.
- The board pictures in `Images/` are generated; regenerate them with
  `make images` when a layout change alters what a render shows, and commit them
  with the change (see "Board images").
- `.gitattributes` and the per-directory `.gitignore` files carry deliberate
  intent (including GitHub language statistics). Read the comments before
  changing them.

### Firmware

- Match the existing style: a single `.ino` file per revision, `#define` pin and
  timing constants at the top in `UPPER_SNAKE_CASE`, `camelCase` functions and
  variables.
- Each release is a **new file** (`V1.9.ino`, `V2.0.ino`, …); never edit
  `V1.0`–`V1.8` in place and never "tidy up" an old revision to match a newer
  one — the differences between versions are the record of what changed.
- If a change affects behavior, update
  [`PIE-v1.8-information.md`](SOFTWARE/Portal%20Interface%20Engine/PIE-v1.8-information.md)
  (or the current version's document) in the same change, and correct
  [`SOFTWARE/README.md`](SOFTWARE/README.md) if board compatibility changes.
  A behavior change with stale documentation is a defect.
- There are two firmware heads: PIE V1.8 for the DEV-Brain (LEDs and button,
  bench-tested) and PIE V1.9 for the main board. V1.9 is the first power-stage
  firmware — arming, PWM dead-time, soft start, closed-loop regulation, and
  supervisory protection — but it has never run on hardware, and neither has the
  power stage. Bench verification is outstanding before either is trusted.

### Python tooling

- The check suite under `tests/` follows the ruff configuration in
  `pyproject.toml`: 100-column lines, double quotes, four-space indents.
  Run `make lint` to check, or `ruff format` to apply formatting.

### Documentation

- Write in plain language and keep it accurate: when documentation and the
  KiCad source disagree, fix the documentation.
- Use relative links so they work in clones as well as on GitHub.
- Don't include vendor datasheets, PDFs, or third-party models — link to them
  instead.

## Testing

Run the repository checks before you open a pull request:

```bash
uv sync        # one-time: create .venv/ from uv.lock
make test      # repository checks (pytest)
make lint      # ruff check and format check
make check     # 'test' plus the KiCad ERC and DRC reports (needs kicad-cli)
make static    # cppcheck over the selected firmware sketch
make firmware  # compile the default firmware sketch (V1.8) with warnings enabled
make firmware SKETCH="SOFTWARE/Portal Interface Engine/V1.9.ino"   # the main-board head
```

The same checks run in CI on every push and pull request
([`.github/workflows/checks.yml`](.github/workflows/checks.yml)), together with
ruff and cppcheck; a red CI run blocks a merge. The repository checks run on
Python 3.11 and 3.14, the firmware build compiles both heads and pins the
Arduino toolchain versions noted at the top of the workflow. `tests/` holds the
check suite: markdown links, BOM contents, the ERC and DRC reports, the two
firmware pin contracts, and a credentials scan.

**Policy:** major new functionality must come with tests, or with an update to
the existing checks, in the same change. A test that only re-states the
implementation is not a test; make it fail before the fix and pass after.

### Hardware changes

Run the electrical and design rule checks and make sure you don't add
violations (or use `make check`, which runs both):

```bash
# Electrical rules check (schematic)
kicad-cli sch erc --output "HARDWARE/Kicadfiles/Lamoka1-half/ERC.rpt" \
  "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_sch"

# Design rules check (PCB), including schematic parity
kicad-cli pcb drc --schematic-parity --output "HARDWARE/Kicadfiles/Lamoka1-half/DRC.rpt" \
  "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pcb"
```

**Verify connectivity by netlist, not by eye.** A change that preserves the
netlist is provably connectivity-neutral. Export the netlist before and after
your change and diff them:

```bash
kicad-cli sch export netlist --format kicadsexpr --output /tmp/before.net \
  "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_sch"
# ... make your change ...
kicad-cli sch export netlist --format kicadsexpr --output /tmp/after.net \
  "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_sch"
diff /tmp/before.net /tmp/after.net
```

If your change touches component values, re-derive the relevant calculations
from the manufacturer datasheet and say so in the pull request.

### Firmware changes

- Build both firmware heads with `make firmware` and
  `make firmware SKETCH="SOFTWARE/Portal Interface Engine/V1.9.ino"` (or the
  Arduino IDE) and keep them free of warnings. CI compiles V1.8 and V1.9 with
  `--warnings all` (`-Wall -Wextra`); warnings from the third-party FastLED
  library are upstream and are noted, but warnings from project code should be
  fixed, not ignored.
- Test on real hardware when you can — the DEV-Brain is the reference board,
  and anything that touches the user interface should at least be exercised on
  it (button gestures, menus, brightness/speed, sleep) before you claim it
  works.
- Say in the pull request what you tested and what you couldn't. Untested code
  isn't rejected; untested code presented as tested is.

### Static analysis

`make static` runs [cppcheck](https://cppcheck.sourceforge.io/) (warning,
performance, and portability checks) over the selected firmware sketch, and CI
runs the same command on every change. Fix findings rather than silencing them.
If a finding is a confirmed false positive, add a file-scoped suppression in the
`Makefile` with a comment explaining why — the existing
`uninitMemberVarNoCtor` suppression for `ModeFire::heat[]` is the example: the
array is `memset()`-cleared by `enter()` before any read, so the finding is a
robustness note rather than an exploitable defect.

### Documentation changes

Check links and formatting (`make test` covers the relative links). If your
change is motivated by a design change, land it together with (or after) that
change.

## Submitting changes

1. **Fork** the repository and create a branch from the `Lamoka1` default
   branch. Use a short, descriptive branch name (`fix-boost-bom`, `pie-v1.9`).
2. **Keep changes focused.** One topic per pull request; small pull requests
   get reviewed faster.
3. **Describe what and why.** Fill in the pull request template: what changed,
   which board/section it affects, how you tested it, and anything you are
   unsure about.
4. **Don't commit generated noise** — editor state, autosaves, backups,
   datasheets, or throwaway exports. The `.gitignore` files are there to help.
5. **Reference issues** with `Fixes #123` / `Closes #123` where applicable.
6. **Be responsive.** Review may ask for the netlist diff, a DRC report, or a
   datasheet reference; that's normal here, not skepticism. If you can prove it,
   show the proof.

The maintainers make the final call on what ships, and the project's human
review and testing standards apply to every contribution.

## Reporting bugs and requesting features

- **Bugs** — use the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md).
  Include the board (Lamoka1/2 or DEV-Brain), firmware version, and what you
  observed versus what you expected. Hardware bugs are easiest to act on with a
  photo or scope trace.
- **Features** — use the
  [feature request template](.github/ISSUE_TEMPLATE/feature_request.md).
- **Security issues and anything safety-critical** — do **not** open a public
  issue; follow [`SECURITY.md`](SECURITY.md).

## Community standards

All participation is covered by the project's
[Code of Conduct](CODE_OF_CONDUCT.md). Be kind, assume good faith, and keep
discussions on the hardware, the firmware, and the goal of making a genuinely
open power-conversion platform.

## Contact

- Email: **flamingoncactusgroup@gmail.com**
- Phone/text: **(607) 205-6663** — text or voicemail only (no calls, please)

For security matters, see [`SECURITY.md`](SECURITY.md) for what to include.
