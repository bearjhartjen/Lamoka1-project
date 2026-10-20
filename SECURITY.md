# Security policy

## Supported versions

| Component | Supported |
| --- | --- |
| PIE firmware (V1.8 for the DEV-Brain, V1.9 for the Lamoka1/2) | Yes |
| PIE V1.0–V1.7 | Removed from the current tree; retained in git history; not maintained |
| Lamoka1/2 main board (current `Lamoka1` branch revision) | Yes |
| Lamoka1-DEV-Brain (current `Lamoka1` branch revision) | Yes |

## Reporting a vulnerability

**Please do not open a public issue for security or safety-critical problems.**

- Email **flamingoncactusgroup@gmail.com** with `SECURITY` in the subject line, or
- Text or leave a voicemail at **(607) 205-6663** (text or voicemail only — please do not call), or
- Use GitHub's private reporting: **Security → Report a vulnerability** on this repository.

Please include:

- what the issue is and where (file, board section, or firmware revision),
- how to reproduce or trigger it,
- the impact you see (including any safety impact),
- and any suggested fix, if you have one.

## Scope

In scope:

- Firmware issues in PIE (crashes, memory corruption, unsafe behavior).
- Repository or infrastructure issues (licensing, publishing, dependency problems).
- **Hardware design issues that could cause unsafe operation** — isolation,
  creepage/clearance, over-current or over-temperature protection, or anything
  that could damage a connected battery or load. The Lamoka1/2 can produce mains
  voltage, so safety-related reports are treated as high priority.

  The design targets 1.5 mm of copper-to-copper clearance between the 120 V
  transformer secondary (`/BLACK`, `/RED`) and every other net — above the
  1.25 mm that IPC-2221B table 6-1 column B2 asks for at 151-300 V working —
  enforced by a rule in `HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_dru`.
  Everything else on the board runs at 21.75 V or below on a 0.18 mm netclass.
  Creepage and clearance reports are welcome against that target.

  The outlet is a 120 V mains socket, and its earth pole is **not** the
  board's logic ground. Rev C cuts the `J5` 3/4-to-`GND` tie, and that cut is
  in the sources: `J5` pins 3/4 are on their own `/PE` net. That net has to
  be bonded in the harness to a chassis PE stud (Class I) or left unbonded
  with the unit marked double-insulated (Class II), so a report on the earth
  handling, or on a unit shipped with a floating or logic-ground-tied earth,
  is in scope and treated as high priority.

Out of scope: issues in third-party tools or libraries, unmodified vendor
components (report those to the manufacturer), and forks of this project.

## What to expect

This project is run by a small team, so response is best-effort — usually
within a few days. We will acknowledge your report, keep you updated as we work
on it, and credit you (with your permission) when a fix ships.
