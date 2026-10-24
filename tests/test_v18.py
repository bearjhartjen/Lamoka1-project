"""Host-side checks for PIE V1.8 (Lamoka1-DEV-Brain UI firmware).

V1.8 drives ten WS2812B LEDs and one button; it contains no power-conversion
code. These tests pin the pin contract, the gesture thresholds, the value
tables, the menu structure, the fixed cursor, and the absence of any power
or networking code, so a future edit that breaks the DEV-Brain contract fails
loudly here instead of on hardware.

Run with ``python3 -m pytest tests/test_v18.py``.
"""

import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH = os.path.join(ROOT, "SOFTWARE", "Portal Interface Engine", "V1.8.ino")

with open(SKETCH, encoding="utf-8") as _handle:
    SOURCE = _handle.read()


def _int(name):
    match = re.search(rf"#define\s+{name}\s+([0-9]+)", SOURCE)
    if match is None:
        raise AssertionError(f"{name} is not defined as an integer in V1.8.ino")
    return int(match.group(1))


class PinContractTests(unittest.TestCase):
    def test_dev_brain_pins(self):
        self.assertEqual(_int("LED_PIN"), 11)
        self.assertEqual(_int("BUTTON_PIN"), 0)
        self.assertEqual(_int("NUM_LEDS"), 10)
        self.assertEqual(_int("COLOR_ENTROPY_PIN"), 5)
        self.assertEqual(_int("ADC0"), 26)
        self.assertEqual(_int("ADC1"), 27)
        self.assertEqual(_int("ADC2"), 28)
        self.assertEqual(_int("ADC3"), 29)

    def test_leds_uses_grb_without_dither(self):
        self.assertIn("FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);", SOURCE)
        self.assertIn("FastLED.setDither(0);", SOURCE)

    def test_no_power_conversion_code(self):
        # V1.8 is UI firmware only: no PWM, no ADC monitor chain, no I2C,
        # no gate drivers, no fault latches, no power service.
        for token in (
            "pwm_",
            "ADS7128",
            "INA240",
            "TMP235",
            "Wire.",
            "GATE_",
            "LM5122",
            "UCC27735",
            "watchdog",
            "setFault",
            "powerService",
        ):
            with self.subTest(token=token):
                self.assertNotIn(token, SOURCE)

    def test_no_min_max_library_calls(self):
        # Strip comments first: the RULE comment itself names min()/max().
        code = re.sub(r"/\*.*?\*/", "", SOURCE, flags=re.DOTALL)
        code = re.sub(r"//.*", "", code)
        hits = [
            m.group(0)
            for m in re.finditer(r"\bmin\s*\(|\bmax\s*\(", code)
            if "maximum" not in code[max(0, m.start() - 8) : m.start()].lower()
        ]
        self.assertEqual(hits, [])


class GestureContractTests(unittest.TestCase):
    def test_gesture_thresholds(self):
        self.assertIn("now - debounceStart < 28", SOURCE)
        self.assertIn("held >= 2800", SOURCE)
        self.assertIn("held >= 1200", SOURCE)
        self.assertIn("held >= 550", SOURCE)
        self.assertIn("lastTap && now - lastTap < 380", SOURCE)
        self.assertIn("held < 400", SOURCE)

    def test_menu_timeout_and_frame(self):
        self.assertEqual(_int("MENU_TIMEOUT_MS"), 16000)
        self.assertEqual(_int("FRAME_MS"), 16)

    def test_brightness_floor(self):
        self.assertEqual(_int("DEFAULT_BRIGHTNESS"), 6)
        self.assertIn("if (b < 6) b = 6;", SOURCE)


class ValueTableTests(unittest.TestCase):
    def test_brightness_values(self):
        block = SOURCE[SOURCE.index("brightVals[10]") :]
        block = block[block.index("{") : block.index("};")]
        values = [int(v) for v in re.findall(r"\b(\d+)\b", block)]
        self.assertEqual(values, [6, 12, 20, 30, 42, 56, 72, 88, 106, 125])

    def test_speed_values(self):
        block = SOURCE[SOURCE.index("speedVals[10]") :]
        block = block[block.index("{") : block.index("};")]
        values = [int(v) for v in re.findall(r"\b(\d+)\b", block)]
        self.assertEqual(values, [40, 55, 70, 85, 100, 120, 145, 170, 200, 240])

    def test_fireball_timings(self):
        for token in (
            "START_SPEED = 115",
            "SPEEDUP     = 10",
            "MIN_SPEED   = 30",
        ):
            with self.subTest(token=token):
                self.assertIn(token, SOURCE)


class MenuStructureTests(unittest.TestCase):
    def test_six_home_items(self):
        menu = SOURCE[SOURCE.index("void renderMainMenu()") :]
        menu = menu[: menu.index("/* ====")]
        for led in range(6):
            with self.subTest(led=led):
                self.assertIn(f"leds[{led}]", menu)
        handle = SOURCE[SOURCE.index("if (uiState == UI_MENU_MAIN) {") :]
        handle = handle[: handle.index("// -------- GAMES MENU --------")]
        self.assertIn("mainCursor + 1) % 6", handle)
        self.assertIn("(mainCursor == 0) ? 5 : mainCursor - 1", handle)

    def test_six_settings_items(self):
        self.assertIn("const uint8_t SETTINGS_COUNT = 6;", SOURCE)

    def test_the_cursor_is_a_fixed_hot_pink(self):
        cursor = SOURCE[SOURCE.index("void renderCursor(uint8_t index) {") :]
        cursor = cursor[: cursor.index("\n}\n")]
        self.assertIn("leds[index] = COLOR_HOT_PINK;", cursor)
        for banned in ("beatsin8(", "entropy.", "CHSV(", "blendColor(", "nscale8("):
            with self.subTest(banned=banned):
                self.assertNotIn(banned, cursor)

    def test_loops_use_the_led_constant(self):
        for banned in (
            "i < 10",
            "p >= 9",
            "rawSource > 9",
            "sourcePos < 9",
            "i < 9",
            "% 11UL",
            "score > 10",
        ):
            with self.subTest(banned=banned):
                self.assertNotIn(banned, SOURCE)


if __name__ == "__main__":
    unittest.main()
