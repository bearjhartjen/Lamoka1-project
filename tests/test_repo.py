"""Repository checks for the Lamoka1 project.

Run from the repository root with `make test`, or directly with pytest:

    uv run pytest -v        # or: python3 -m pytest -v

The checks need Python 3, pytest, and the repository itself: they verify that
the documentation, generated reports, BOM, and firmware sources stay consistent
with each other. Design-rule checking (KiCad) and static analysis (cppcheck)
are separate targets: `make check` and `make static`.
"""

import csv
import os
import re
import subprocess
import unittest
import urllib.parse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PIE_DIR = os.path.join(ROOT, "SOFTWARE", "Portal Interface Engine")
BOARD_DIR = os.path.join(ROOT, "HARDWARE", "Kicadfiles", "Lamoka1-half")


def repo_path(*parts):
    """Absolute path for a repo-relative path given as separate parts."""
    return os.path.join(ROOT, *parts)


def read_text(path):
    """Read a text file from the repository."""
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def tracked_files():
    """Repo-relative paths of the files git tracks.

    Falls back to walking the tree when git is not available (for example in a
    source tarball).
    """
    try:
        result = subprocess.run(
            ["git", "ls-files", "-z"],
            cwd=ROOT,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
    except (OSError, subprocess.CalledProcessError):
        paths = []
        for dirpath, dirnames, filenames in os.walk(ROOT):
            dirnames[:] = [d for d in dirnames if d != ".git"]
            for name in filenames:
                paths.append(os.path.relpath(os.path.join(dirpath, name), ROOT))
        return paths
    return [p for p in result.stdout.decode("utf-8", "replace").split("\0") if p]


class DocumentationTests(unittest.TestCase):
    """The project's core documents exist, have content, and link correctly."""

    REQUIRED = [
        "README.md",
        "CHANGELOG.md",
        "CONTRIBUTING.md",
        "CODE_OF_CONDUCT.md",
        "SECURITY.md",
        "docs/README.md",
        ".github/PULL_REQUEST_TEMPLATE.md",
        ".github/workflows/checks.yml",
        ".github/dependabot.yml",
        ".editorconfig",
        ".env.example",
        ".python-version",
        "pyproject.toml",
        "uv.lock",
        "LICENSE-HARD",
        "LICENSE-SOFT",
        "LICENSES/CERN-OHL-W-2.0.txt",
        "LICENSES/GPL-3.0-only.txt",
        "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_sch",
        "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_pcb",
        "HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.csv",
        "HARDWARE/Kicadfiles/Lamoka1-half/ERC.rpt",
        "HARDWARE/Kicadfiles/Lamoka1-half/DRC.rpt",
        "HARDWARE/Mechanical/Lamoka1-half.step",
        "HARDWARE/Mechanical/Lamoka1-DEV-Brain.step",
        "HARDWARE/Mechanical/README.md",
        "SOFTWARE/Portal Interface Engine/PIE-v1.8-information.md",
        "SOFTWARE/Portal Interface Engine/PIE-v1.9-information.md",
    ]

    def test_required_files_exist(self):
        missing = [
            path for path in self.REQUIRED if not os.path.isfile(repo_path(*path.split("/")))
        ]
        self.assertEqual([], missing, f"missing required files: {missing}")

    def test_core_documents_are_not_stubs(self):
        for name in (
            "README.md",
            "CHANGELOG.md",
            "CONTRIBUTING.md",
            "CODE_OF_CONDUCT.md",
            "SECURITY.md",
            "docs/README.md",
        ):
            with self.subTest(document=name):
                self.assertGreater(
                    len(read_text(repo_path(*name.split("/")))),
                    1000,
                    f"{name} looks like a stub",
                )

    def test_relative_markdown_links_resolve(self):
        broken = []
        for path in tracked_files():
            if not path.lower().endswith(".md"):
                continue
            directory = os.path.dirname(repo_path(*path.split("/")))
            text = read_text(repo_path(*path.split("/")))
            # Markdown destinations come in two shapes: `](plain/path)` and
            # `](<path with spaces>)`.
            targets = re.findall(r"\]\(<([^>]+)>", text)
            targets += re.findall(r"\]\(([^)<\s][^)\s]*)", text)
            for target in targets:
                if target.startswith(("http://", "https://", "mailto:", "#")):
                    continue
                clean = urllib.parse.unquote(target.split("#", 1)[0])
                if clean and not os.path.exists(os.path.normpath(os.path.join(directory, clean))):
                    broken.append(f"{path} -> {target}")
        self.assertEqual([], broken, "broken relative links:\n" + "\n".join(broken))

    def test_readme_images_exist(self):
        missing = []
        for src in re.findall(r'<img[^>]*src="([^"]+)"', read_text(repo_path("README.md"))):
            if src.startswith(("http://", "https://")):
                continue
            if not os.path.exists(repo_path(*urllib.parse.unquote(src).split("/"))):
                missing.append(src)
        self.assertEqual([], missing, f"missing README images: {missing}")


class LicenseTests(unittest.TestCase):
    """The SPDX license texts are present, complete, and in sync."""

    def test_license_texts_are_complete(self):
        gpl = read_text(repo_path("LICENSES", "GPL-3.0-only.txt"))
        self.assertIn("GNU GENERAL PUBLIC LICENSE", gpl)
        self.assertIn("Version 3, 29 June 2007", gpl)
        self.assertGreater(len(gpl), 30000, "GPL text is truncated")
        cern = read_text(repo_path("LICENSES", "CERN-OHL-W-2.0.txt"))
        self.assertIn("CERN Open Hardware Licence Version 2 - Weakly Reciprocal", cern)
        self.assertGreater(len(cern), 10000, "CERN-OHL-W text is truncated")

    def test_root_copies_match_the_licenses_directory(self):
        self.assertEqual(
            read_text(repo_path("LICENSES", "GPL-3.0-only.txt")),
            read_text(repo_path("LICENSE-SOFT")),
            "LICENSES/GPL-3.0-only.txt differs from LICENSE-SOFT",
        )
        self.assertEqual(
            read_text(repo_path("LICENSES", "CERN-OHL-W-2.0.txt")),
            read_text(repo_path("LICENSE-HARD")),
            "LICENSES/CERN-OHL-W-2.0.txt differs from LICENSE-HARD",
        )


class HardwareReportTests(unittest.TestCase):
    """The committed fabrication data stays consistent and complete."""

    def test_bom_parses_and_covers_parts(self):
        with open(
            repo_path("HARDWARE", "Kicadfiles", "Lamoka1-half", "Lamoka1-half.csv"),
            encoding="utf-8",
        ) as handle:
            rows = list(csv.DictReader(handle))
        self.assertGreaterEqual(len(rows), 35, "the BOM has suspiciously few rows")
        for row in rows:
            with self.subTest(reference=row.get("Reference")):
                self.assertTrue((row.get("Reference") or "").strip())
                self.assertTrue((row.get("Value") or "").strip())
        with_mpn = sum(1 for row in rows if (row.get("MPN") or "").strip())
        self.assertGreaterEqual(
            with_mpn / len(rows),
            0.85,
            "too many BOM rows lost their MPN (the connectors are sourced by value)",
        )

    def test_erc_report_has_no_errors(self):
        text = read_text(repo_path("HARDWARE", "Kicadfiles", "Lamoka1-half", "ERC.rpt"))
        match = re.search(r"Errors\s+(\d+)", text)
        self.assertIsNotNone(match, "ERC report has no error summary; regenerate it")
        self.assertEqual(0, int(match.group(1)), "the ERC report contains errors")

    def test_drc_report_has_no_unconnected_pads(self):
        text = read_text(repo_path("HARDWARE", "Kicadfiles", "Lamoka1-half", "DRC.rpt"))
        match = re.search(r"Found\s+(\d+)\s+unconnected (?:pads|items)", text)
        self.assertIsNotNone(match, "DRC report has no unconnected-pads summary; regenerate it")
        self.assertEqual(0, int(match.group(1)), "the PCB has unconnected pads")

    def test_step_models_are_present(self):
        for name in (
            "Lamoka1-half.step",
            "Lamoka1-DEV-Brain.step",
        ):
            with self.subTest(model=name):
                path = repo_path("HARDWARE", "Mechanical", name)
                self.assertGreater(os.path.getsize(path), 100_000, f"{name} looks truncated")


class FirmwareTests(unittest.TestCase):
    """The firmware revisions and the documented pin contract are intact."""

    def test_all_pie_revisions_present(self):
        names = ["V1.ino"] + [f"V1.{n}.ino" for n in range(1, 10)]
        for name in names:
            with self.subTest(sketch=name):
                self.assertTrue(os.path.isfile(os.path.join(PIE_DIR, name)), f"{name} is missing")

    def test_pin_contract_of_dev_brain_head(self):
        # Keep in sync with PIE-v1.8-information.md and AGENTS.md: GPIO 11 is
        # LED data, GPIO 0 is the button, and the DEV-Brain fits ten LEDs.
        text = read_text(os.path.join(PIE_DIR, "V1.8.ino"))
        for pattern in (
            r"#define\s+LED_PIN\s+11\b",
            r"#define\s+BUTTON_PIN\s+0\b",
            r"#define\s+NUM_LEDS\s+10\b",
        ):
            with self.subTest(pattern=pattern):
                self.assertRegex(text, pattern)
        self.assertIn("FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);", text)

    def test_pin_contract_of_main_board_head(self):
        # Keep in sync with PIE-v1.9-information.md and AGENTS.md, and with the
        # main-board netlist: five LEDs, GPIO 0 button, GPIO 1-4 to the
        # UCC27735 gate-driver inputs, I2C on GPIO 8/9, TMP235 on GPIO 26/27.
        text = read_text(os.path.join(PIE_DIR, "V1.9.ino"))
        for pattern in (
            r"#define\s+LED_PIN\s+11\b",
            r"#define\s+BUTTON_PIN\s+0\b",
            r"#define\s+NUM_LEDS\s+5\b",
            r"#define\s+TEMP_CTRL\s+26\b",
            r"#define\s+TEMP_INV\s+27\b",
            r"#define\s+GATE_A_HI_PIN\s+1\b",
            r"#define\s+GATE_A_LO_PIN\s+2\b",
            r"#define\s+GATE_B_HI_PIN\s+3\b",
            r"#define\s+GATE_B_LO_PIN\s+4\b",
        ):
            with self.subTest(pattern=pattern):
                self.assertRegex(text, pattern)
        self.assertIn("Wire.setSDA(8);", text)
        self.assertIn("Wire.setSCL(9);", text)
        self.assertIn("FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);", text)


class CredentialTests(unittest.TestCase):
    """No obvious secrets are committed to the repository."""

    PATTERNS = [
        re.compile(rb"-----BEGIN (?:RSA |DSA |EC |OPENSSH |PGP )?PRIVATE KEY-----"),
        re.compile(rb"AKIA[0-9A-Z]{16}"),
        re.compile(rb"gh[pousr]_[A-Za-z0-9]{36,}"),
        re.compile(rb"xox[baprs]-[A-Za-z0-9-]{10,}"),
    ]

    def test_no_obvious_credentials(self):
        hits = []
        for path in tracked_files():
            try:
                with open(repo_path(*path.split("/")), "rb") as handle:
                    data = handle.read()
            except OSError:
                continue
            for pattern in self.PATTERNS:
                if pattern.search(data):
                    hits.append(f"{path}: matches {pattern.pattern!r}")
        self.assertEqual([], hits, "possible credentials committed:\n" + "\n".join(hits))


if __name__ == "__main__":
    unittest.main()
