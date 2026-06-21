<div align="center">

# 🤝 Contributing to Lamoka

**An open-source ecosystem of power conversion hardware and firmware**
*Managed by Flamingo and Cactus*

[![Firmware License](https://img.shields.io/badge/Firmware-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.html)
[![Hardware License](https://img.shields.io/badge/Hardware-CERN--OHL--W%20v2.0-orange.svg)](https://ohwr.org/cern_ohl_w_v2.txt)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](#how-to-contribute)

</div>

---

## 📋 Table of Contents

- [Before You Start](#-before-you-start)
- [How to Contribute](#-how-to-contribute)
- [Licensing](#-licensing)
- [Documenting Your Contribution](#-documenting-your-contribution)
- [Guidelines](#-guidelines)
- [Code of Conduct](#-code-of-conduct)
- [Questions?](#-questions)

This guide applies across **all products** in the Lamoka ecosystem — Lamoka1 and everything that comes after it.

---

## 🚦 Before You Start

> [!IMPORTANT]
> For anything beyond a small fix — new features, hardware revisions, architectural changes — **open an issue first.** It saves you from doing work that might not align with the project's direction, and gives us a chance to talk through the approach together.

---

## 🛠️ How to Contribute

1. **Open an issue** — describe what you want to change and why.
2. **Fork the repo** and create a branch for your work.
3. **Make your changes**, following the licensing and documentation rules below.
4. **Add an `ADDITION.md`** in the exact folder where you made your change.
5. **Submit a pull request** to `main` with a clear description of what changed and why.

---

## ⚖️ Licensing

Lamoka splits licensing by content type:

| Content Type | License | Applies To |
|:---|:---|:---|
| 💾 **Firmware / software** | [GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html) | Any `*software` folder (e.g. `Lamoka1software`) |
| 🔧 **Hardware designs** | [CERN-OHL-W v2.0](https://ohwr.org/cern_ohl_w_v2.txt) | Any `*hardware` folder (e.g. `lamoka1hardware`) |

The applicable license file lives in each relevant folder (`LICENSE-SOFT` / `LICENSE-HARD`). Read the one that covers the area you're touching — the two have different obligations:

> [!NOTE]
> **GPL 3.0 (firmware):** If you distribute software built on this code, you must make the corresponding source available under GPL 3.0 too.
>
> **CERN-OHL-W (hardware):** If you distribute a product — or a modified version of the design itself — you must make the modified design files available under CERN-OHL-W. Just *using* a Lamoka-derived product without distributing it doesn't trigger this.

> [!TIP]
> This is a plain-language summary, not legal advice. When in doubt, check the full license text.

### Your Copyright

**You keep copyright to your contribution.** Submitting a PR grants Lamoka a perpetual, worldwide, royalty-free license to use, modify, and redistribute it under whichever license governs that area (GPL 3.0 for firmware, CERN-OHL-W for hardware). Standard "inbound = outbound" model — you're not giving anything up, just matching the terms already in place.

---

## 📝 Documenting Your Contribution

Every contribution needs an `ADDITION.md` placed **only** in the folder where the change happened — not at the repo root, not duplicated elsewhere. One file, one location, one clear record of who changed what.

Each `ADDITION.md` should include:

- ✏️ **Your name** (or handle)
- 📄 **A short description** of the change
- ⚖️ **The license** it falls under
- 📅 **The date**

<details>
<summary><b>📄 Click to see an example ADDITION.md</b></summary>

```markdown
# Addition

**Contributor:** Jane Doe
**Date:** 2026-06-17
**License:** CERN Open Hardware Licence v2.0 (Weakly Reciprocal)

## Change

Added reverse-polarity protection to the input stage of the boost converter,
using a P-channel MOSFET instead of a series diode to reduce conduction losses.
```

</details>

If your PR spans multiple folders, add a separate `ADDITION.md` in each one rather than summarizing everything in a single file.

---

## ✅ Guidelines

- **Code style:** Match what's already there in the file/folder you're editing.
- **Documentation:** Adding a component, feature, or folder? Update the relevant `README.md` so it doesn't go stale.
- **Hardware/PCB files:** Keep schematics, layouts, 3D models, and footprint libraries consistent — and make sure DRC passes before submitting.
- **Commit messages:** Be specific. *"Fix slope-comp resistor value for 250kHz phase"* > *"update stuff."*

---

## 🌱 Code of Conduct

Be respectful. Lamoka should be a welcoming, collaborative project no matter someone's experience level — questions are always welcome in Issues or Discussions.

---

## 💬 Questions?

Open an issue or start a thread in **Discussions** — we're happy to help.
