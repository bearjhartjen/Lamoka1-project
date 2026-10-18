## Summary

<!-- One or two sentences: what does this change, and why? -->

## Area

- [ ] Hardware (schematic, layout, fabrication files)
- [ ] Firmware (Portal Interface Engine)
- [ ] Documentation
- [ ] Mechanical (STEP/STL, enclosure)
- [ ] Other:

## Related issues

<!-- e.g. "Fixes #12" -->

## How has this been tested?

<!-- "Not tested" is an acceptable answer — but it must be the honest one. -->

- [ ] ERC run, no new violations
- [ ] DRC run (with schematic parity), no new violations
- [ ] Netlist diffed before/after (required for connectivity-adjacent hardware changes)
- [ ] Sketch compiles for RP2350 (`arduino-cli compile --fqbn rp2040:rp2040:generic_rp2350 ...`)
- [ ] Tested on real hardware — board: ______ / firmware version: ______
- [ ] Documentation updated alongside behavior changes (design notes, PIE information, README)

## Checklist

- [ ] My change follows the conventions in [CONTRIBUTING.md](../CONTRIBUTING.md)
- [ ] I have not committed editor state, autosaves, backups, datasheets, or vendor files
- [ ] I understand and stand behind every line of this change, including any AI-assisted parts

## Notes for reviewers

<!-- Anything you're unsure about, or want a second opinion on. -->
