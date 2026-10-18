# Security policy

## Supported versions

| Component | Supported |
| --- | --- |
| PIE firmware (V1.8, current head) | Yes |
| PIE V1.0–V1.7 | Kept for reference and compatibility; not maintained |
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

Out of scope: issues in third-party tools or libraries, unmodified vendor
components (report those to the manufacturer), and forks of this project.

## What to expect

This project is run by a small team, so response is best-effort — usually
within a few days. We will acknowledge your report, keep you updated as we work
on it, and credit you (with your permission) when a fix ships.
