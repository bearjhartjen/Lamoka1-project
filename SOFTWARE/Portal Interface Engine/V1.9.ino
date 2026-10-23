#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include <math.h>
#include "pico/bootrom.h"
#include "pico/rand.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"

/* Forward declarations for the power stage (defined later in this file). */
void powerBegin();
void powerService(uint32_t nowMs);
bool powerFaulted();
void powerClearFault();
bool powerArmed();
bool powerSensorsOk();
bool powerSetArmed(bool on);
uint8_t powerState();
const char *powerStateName();
float powerBusVolts();
float powerOutputVolts();
float powerInputAmps();
void renderInverter();
static float sineAt(uint32_t phase);
// Forward declarations so the Arduino-generated prototypes (inserted above
// the first function definition) can name these types before their
// definitions further down the file.
struct MidSample;
struct FundamentalFit;

/*
 * ================================================================
 * LAMOKA1/2 MAIN BOARD — PORTAL INTERFACE ENGINE V1.9
 * ================================================================
 *
 First PIE release that runs on the main Lamoka1/2 board
 * the H-bridge power stage.  Derived from V1.8 (DEV-Brain UI firmware);
 * the UI gestures, effects and style are preserved and scaled to the
 * main board's five WS2812B LEDs.
 *
 * POWER TOPOLOGY
 * --------------
 * 11-14 V in -> Rsense1 -> LM5122 boost -> 21.75 V bus -> UCC27735
 * gate drivers -> Q3/Q4/Q5/Q6 H-bridge -> 12 V winding of a Triad
 * VPT24-6670 -> 115 V winding -> 120.0 V RMS regulated outlet.
 *
 * The outlet is regulated by ESTIMATE: a droop-compensated feed forward
 * from the measured drive winding and the estimated load, closed by a
 * slow integral loop on that estimate.  The board has no isolated output
 *  sense, so a direct isolated measurement of the outlet comes with a
 *  later Lamoka1 model.
 *
 * HOME — DUAL PERSONALITY
 * ------------------------
 * USB 5 V alone (input rail ~0 V) is 5V mode: Status, Fireplace,
 * Lightsaber, Rainbow, Settings — the toys, V1.8 colors.  A battery on
 * the input rail flips the board into inverter mode: Status (blue),
 * Inverter (purple), Settings (amber).  The flip disarms and returns to
 * idle; arming is refused in 5V mode.
 *
 * CURSOR
 * ------
 * Every selected item is hot pink (steady, no pulse).
 * Cursors remember the last selected item.
 *
 * DEFAULT IDLE
 * ------------
 * Blue bouncing status ball.
 *
 * 5V MODE TOYS
 * ------------
 * USB power alone (no battery) offers Status, Fireplace, Lightsaber,
 * Rainbow and Settings — the V1.8 set, scaled to five LEDs.  The battery
 * flips the board into inverter mode, where the portal replaces them.
 *
 * BRIGHTNESS
 * ----------
 * Hard floor = 6
 *
 * BOOTSEL
 * -------
 * reset_usb_boot(0, 0)
 *
 * RULE
 * ----
 * No min() / max() library calls anywhere.
 */

/* ================================================================
 * HARDWARE
 * ================================================================
 * The main Lamoka1/2 board fits five WS2812B LEDs.  GPIO 26/27 are
 * TMP235 temperature sensors, not entropy sources.  GPIO 5/28/29 are
 * unconnected and must not be driven.
 * ================================================================ */
#define LED_PIN           11
#define NUM_LEDS          5
#define BUTTON_PIN        0

#define TEMP_CTRL         26   // TMP235, control-section temperature
#define TEMP_INV          27   // TMP235, inversion-section temperature

#define DEFAULT_BRIGHTNESS 6
#define DEFAULT_SPEED      100
#define FRAME_MS           16
#define MENU_TIMEOUT_MS    16000UL

CRGB leds[NUM_LEDS];

/* ================================================================
 * COLORS
 * ================================================================ */
const CRGB COLOR_CACTUS   = CRGB(0, 90, 18);
const CRGB COLOR_HOT_PINK = CRGB(255, 40, 160);
const CRGB COLOR_SETTINGS = CRGB(0, 50, 100);
// Inverter-mode home items: status blue, inverter purple, settings amber.
const CRGB COLOR_PORTAL_BLUE   = CRGB(0, 70, 255);
const CRGB COLOR_PORTAL_PURPLE = CRGB(170, 60, 255);
const CRGB COLOR_PORTAL_AMBER  = CRGB(255, 150, 20);


/* ================================================================
 * ENUMERATIONS
 * ================================================================ */
enum Gesture : uint8_t {
  G_NONE,
  G_TAP,
  G_DOUBLE,
  G_HOLD,
  G_LONG_HOLD,
  G_VERY_LONG
};

enum UIState : uint8_t {
  UI_MODE,
  UI_MENU_MAIN,
  UI_MENU_SETTINGS,
  UI_BRIGHTNESS,
  UI_SPEED,
  UI_INFO,
  UI_INVERTER,
  UI_SLEEP
};

/* ================================================================
 * GLOBAL SETTINGS
 * ================================================================ */
uint8_t globalBrightness = DEFAULT_BRIGHTNESS;
uint8_t globalSpeed      = DEFAULT_SPEED;

/* ================================================================
 * UI GLOBALS  (cursors now persist)
 * ================================================================ */
uint8_t mainCursor           = 0;
// Dual personality.  USB 5 V alone runs the MCU, the LEDs and the ADS7128,
// but the input rail reads ~0 V: that is 5V mode (toys, no inverter).  A
// battery on the input rail flips the board into inverter mode, where the
// toy modes are replaced by the Inverter portal.  Hysteresis keeps a
// sagging battery from flapping the UI.  Leaving inverter mode disarms but
// latches nothing; a sag that stays between 9.0 V and the 10.8 V rail limit
// latches an input-rail fault instead.
static bool inverterPower = false;   // latched, hysteresis below
#define INVERTER_ENTER_V  10.0f
#define INVERTER_EXIT_V   9.0f
uint8_t settingsCursor       = 0;   // remembers last Settings selection
uint8_t currentMainMode      = 0;
uint8_t settingsReturnCursor = 0;
uint32_t menuLastActivity    = 0;
UIState uiState              = UI_MODE;

/* ================================================================
 * LED HELPERS
 * ================================================================ */
void applyBrightness() {
  uint8_t b = globalBrightness;
  if (b < 6) b = 6;
  FastLED.setBrightness(b);
}

void clearLeds() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
}

CRGB blendColor(const CRGB &a, const CRGB &b, uint8_t amount) {
  return CRGB(
    lerp8by8(a.r, b.r, amount),
    lerp8by8(a.g, b.g, amount),
    lerp8by8(a.b, b.b, amount)
  );
}

void renderCursor(uint8_t index) {
  if (index >= NUM_LEDS) return;
  // The cursor is a fixed hot pink, always.  It carries no entropy, no hue
  // variation and no brightness breathing: on the DEV-Brain the pink was seen
  // drifting toward near-white, which reads as a system bug.  Selection must
  // never look like a fault.
  leds[index] = COLOR_HOT_PINK;
}

/* ================================================================
 * BUTTON SERVICE
 * ================================================================ */
class ButtonService {
  bool lastRaw         = HIGH;
  bool wasPressed      = false;
  bool pendingTap      = false;
  bool holdFired       = false;
  bool longFired       = false;
  bool veryLongFired   = false;
  uint32_t pressStart  = 0;
  uint32_t debounceStart = 0;
  uint32_t lastTap     = 0;

public:
  void begin() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    lastRaw = digitalRead(BUTTON_PIN);
    debounceStart = millis();
  }

  Gesture poll(bool inMenu) {
    bool raw = digitalRead(BUTTON_PIN);
    uint32_t now = millis();

    if (raw != lastRaw) {
      debounceStart = now;
      lastRaw = raw;
    }
    if (now - debounceStart < 28) return G_NONE;

    bool pressed = !raw;

    if (pressed && !wasPressed) {
      wasPressed = true;
      pressStart = now;
      holdFired = longFired = veryLongFired = false;
      return G_NONE;
    }

    if (pressed) {
      uint32_t held = now - pressStart;
      if (held >= 2800 && !veryLongFired) {
        veryLongFired = true;
        return G_VERY_LONG;
      }
      if (held >= 1200 && !longFired) {
        longFired = true;
        return G_LONG_HOLD;
      }
      if (held >= 550 && !holdFired) {
        holdFired = true;
        return G_HOLD;
      }
    }

    if (!pressed && wasPressed) {
      wasPressed = false;
      uint32_t held = now - pressStart;
      if (!holdFired && !longFired && !veryLongFired && held < 400) {
        if (inMenu) return G_TAP;
        if (lastTap && now - lastTap < 380) {
          lastTap = 0;
          pendingTap = false;
          return G_DOUBLE;
        }
        lastTap = now;
        pendingTap = true;
      }
    }

    if (!inMenu && pendingTap && lastTap && now - lastTap >= 380) {
      pendingTap = false;
      return G_TAP;
    }

    return G_NONE;
  }
};

ButtonService button;

/* ================================================================
 * BASE MODE
 * ================================================================ */
class Mode {
public:
  virtual ~Mode() {}
  virtual void enter() {}
  virtual void update(Gesture) {}
  virtual void render() {}
};

/* ================================================================
 * STATUS — BLUE BOUNCING BALL
 * ================================================================ */
class ModeStatus : public Mode {
  int8_t p = 0;
  int8_t d = 1;
  uint32_t last = 0;

public:
  void enter() override {
    p = 0;
    d = 1;
    last = millis();
  }

  void render() override {
    clearLeds();
    uint16_t iv = (uint16_t)(28 + 8000 / (globalSpeed + 50));
    if (millis() - last >= iv) {
      last = millis();
      p += d;
      if (p >= NUM_LEDS - 1) { p = NUM_LEDS - 1; d = -1; }
      if (p <= 0) { p = 0; d = 1; }
    }

    for (int t = 0; t < 3; t++) {
      int q = p - d * t;
      if (q >= 0 && q < NUM_LEDS) {
        leds[q] = CRGB(0, 25 + t * 35, 70 + t * 55);
      }
    }
    leds[p] = CRGB(0, 70, 255);
  }
};

/* ================================================================
 * ENTROPY SERVICE  (RP2350 hardware TRNG)
 * ================================================================
 * Only the 5V lighting modes consume randomness.  The TMP235 analog
 * channels are temperature sensors, not entropy sources, and nothing in
 * the power stage or the interface needs randomness.
 */
class EntropyService {
  uint32_t s = 1;

  void mix() {
    uint32_t r = (uint32_t)get_rand_64();
    s ^= r;
    s ^= micros();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    s += 0x9E3779B9UL;
  }

public:
  void begin() {
    s ^= micros();
    for (uint16_t i = 0; i < 300; i++) mix();
  }

  void update() { mix(); }

  uint32_t next() {
    mix();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }

  uint8_t next8() { return (uint8_t)(next() & 255UL); }

  uint16_t range(uint16_t maximum) {
    if (maximum == 0) return 0;
    return (uint16_t)(next() % ((uint32_t)maximum + 1UL));
  }

  bool coin() { return (next() & 1UL) != 0; }
};

EntropyService entropy;

/* ================================================================
 * FIREPLACE — 5-LED port of the V1.8 fire
 * ================================================================ */
class ModeFire : public Mode {
  uint8_t heat[NUM_LEDS];
  int16_t source = 2;
  int16_t target = 2;
  int8_t  sourceVel = 0;
  uint32_t lastMove = 0;
  uint8_t flare = 0;

  void chooseTarget() {
    target = (int16_t)(1 + entropy.range(NUM_LEDS - 3));   // targets 1..N-2
  }

public:
  void enter() override {
    memset(heat, 0, sizeof(heat));
    source = target = NUM_LEDS / 2;
    sourceVel = 0;
    lastMove = millis();
    flare = 0;
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      heat[i] = (uint8_t)(30 + entropy.range(50));
    }
  }

  void update(Gesture g) override {
    if (g == G_TAP) {
      flare = 160;
    }
    if (g == G_DOUBLE) {
      flare = 240;
      chooseTarget();
      sourceVel = (int8_t)(entropy.range(5) - 2);
    }
  }

  void render() override {
    uint32_t now = millis();

    if (now - lastMove > 180UL + entropy.range(220)) {
      chooseTarget();
      lastMove = now;
      sourceVel = (int8_t)(entropy.range(3) - 1);
    }

    if (source < target) source++;
    else if (source > target) source--;
    source += sourceVel;
    if (source < 1)  { source = 1;  sourceVel = 0; }
    if (source > NUM_LEDS - 2) { source = NUM_LEDS - 2; sourceVel = 0; }   // top LED: sparks only
    if (sourceVel) sourceVel = sourceVel > 0 ? sourceVel - 1 : sourceVel + 1;

    uint8_t wind1 = inoise8(now / 70);
    uint8_t wind2 = inoise8(now / 140 + 50);
    int8_t drift = (int8_t)(((int)wind1 - 128) / 28 + ((int)wind2 - 128) / 40);

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      uint8_t cooling = (uint8_t)(2 + entropy.range(3) + (i * 3) / 8);
      heat[i] = heat[i] > cooling ? heat[i] - cooling : 0;
    }

    int16_t rawSource = source + drift;
    if (rawSource < 0) rawSource = 0;
    if (rawSource > NUM_LEDS - 1) rawSource = NUM_LEDS - 1;
    uint8_t sourcePos = (uint8_t)rawSource;

    uint8_t fuel = (uint8_t)(120 + entropy.range(70) + (flare ? 40 : 0));
    heat[sourcePos] = qadd8(heat[sourcePos], fuel);
    if (sourcePos > 0) heat[sourcePos - 1] = qadd8(heat[sourcePos - 1], fuel / 2);
    if (sourcePos < NUM_LEDS - 1) heat[sourcePos + 1] = qadd8(heat[sourcePos + 1], fuel / 3);

    uint8_t old[NUM_LEDS];
    memcpy(old, heat, sizeof(heat));
    for (uint8_t i = 1; i < NUM_LEDS - 1; i++) {
      uint16_t average = (uint16_t)old[i - 1] + (old[i] * 2) + old[i + 1];
      heat[i] = lerp8by8(old[i], (uint8_t)(average / 4), 160);
    }

    for (int i = NUM_LEDS - 1; i >= 1; i--) {
      uint16_t rising = (uint16_t)heat[i - 1] * 3 / 2;
      if (i >= 2) rising += heat[i - 2] / 2;
      heat[i] = lerp8by8(heat[i], (uint8_t)(rising > 255 ? 255 : rising), 180);
    }

    if (entropy.next8() > 200) {
      int q = sourcePos + (int)entropy.range(4) - 1;
      if (q < 1) q = 1;
      if (q > NUM_LEDS - 2) q = NUM_LEDS - 2;
      uint8_t f = (uint8_t)(70 + entropy.range(120));
      heat[q] = qadd8(heat[q], f);
      if (q < NUM_LEDS - 1) heat[q + 1] = qadd8(heat[q + 1], f / 2);
      if (q < NUM_LEDS - 2 && entropy.coin()) heat[q + 2] = qadd8(heat[q + 2], f / 4);
    }

    // High sparks live in the upper fire, like V1.8's LEDs 5-8 of ten:
    // normalized 0.55-0.89 of the strip is LEDs 2-4 of five.
    if (entropy.next8() > 245) {
      uint8_t spark = (uint8_t)(2 + entropy.range(2));
      heat[spark] = qadd8(heat[spark], (uint8_t)(100 + entropy.range(80)));
    }

    if (flare) {
      heat[sourcePos] = qadd8(heat[sourcePos], flare);
      if (sourcePos < NUM_LEDS - 1) heat[sourcePos + 1] = qadd8(heat[sourcePos + 1], flare * 2 / 3);
      if (sourcePos < NUM_LEDS - 2) heat[sourcePos + 2] = qadd8(heat[sourcePos + 2], flare / 3);
      flare = flare > 9 ? flare - 9 : 0;
    }

    if (heat[0] < 40) heat[0] = 40 + entropy.range(20);

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      uint8_t noise = inoise8(now / 32 + i * 41);
      heat[i] = scale8(heat[i], (uint8_t)(210 + (noise >> 4)));
      uint8_t h = heat[i];

      if (h < 20) {
        leds[i] = blendColor(CRGB(4, 0, 0), CRGB(30, 1, 0), (uint8_t)(h * 12));
      } else if (h < 55) {
        leds[i] = blendColor(CRGB(30, 1, 0), CRGB(120, 8, 0),
                             scale8((uint8_t)(h - 20), 255));
      } else if (h < 100) {
        leds[i] = blendColor(CRGB(120, 8, 0), CRGB(255, 40, 0),
                             scale8((uint8_t)(h - 55), 255));
      } else if (h < 155) {
        leds[i] = blendColor(CRGB(255, 40, 0), CRGB(255, 140, 10),
                             scale8((uint8_t)(h - 100), 255));
      } else if (h < 210) {
        leds[i] = blendColor(CRGB(255, 140, 10), CRGB(255, 220, 60),
                             scale8((uint8_t)(h - 155), 255));
      } else {
        leds[i] = blendColor(CRGB(255, 220, 60), CRGB(255, 250, 200),
                             scale8((uint8_t)(h - 210), 255));
      }
    }
  }
};

/* ================================================================
 * LIGHTSABER — 5-LED port of the V1.8 blade
 * ================================================================ */
class ModeSaber : public Mode {
  uint8_t hue = 0;
  uint8_t lastHue = 255;
  uint8_t sat = 220;
  uint8_t pattern = 0;
  uint8_t instability = 30;
  uint8_t power = 0;
  uint8_t clash = 0;
  bool extending = true;
  uint32_t ignition = 0;

  void launch() {
    do {
      hue = entropy.next8();
    } while (hue == lastHue);
    lastHue = hue;

    sat = (uint8_t)(175 + entropy.range(65));
    pattern = (uint8_t)entropy.range(6);
    instability = (uint8_t)(20 + entropy.range(80));
    power = 0;
    extending = true;
    ignition = millis();
  }

public:
  void enter() override { launch(); }

  void update(Gesture g) override {
    if (g == G_TAP) {
      if (extending) extending = false;
      else launch();
    }
    if (g == G_DOUBLE) launch();
    if (g == G_HOLD) clash = 16;
  }

  void render() override {
    clearLeds();
    uint8_t rate = (uint8_t)(7 + globalSpeed / 30);
    if (pattern == 1) rate += 4;
    if (pattern == 3 && rate > 2) rate--;

    if (extending) power = qadd8(power, rate);
    else           power = qsub8(power, 10);

    uint8_t len = scale8(power, NUM_LEDS);

    for (uint8_t i = 0; i < len; i++) {
      uint8_t b = qsub8(255, (uint8_t)(i * 22));

      if (pattern == 3 && entropy.next8() < instability) b = scale8(b, 120);
      if (pattern == 5 && entropy.next8() > 220)         b = 255;
      if (pattern == 4) b = scale8(b, beatsin8(12, 145, 255));
      if (pattern == 2) b = scale8(b, beatsin8(9, 185, 255));
      if (pattern == 1 && i == 0) b = 255;

      leds[i] = CHSV(hue, sat, b);
      if (i < 2) {
        leds[i] = blendColor(leds[i], CRGB::White, pattern == 1 ? 95 : 75);
      }
    }

    if (extending && millis() - ignition < 170 && len) {
      leds[0] = blendColor(leds[0], CRGB::White, 180);
    }

    if (clash) {
      for (uint8_t i = 0; i < len; i++) {
        if (entropy.next8() > 90) {
          leds[i] = blendColor(leds[i], CRGB::White, 210);
        }
      }
      clash--;
    }

    if (len && entropy.next8() > 220) {
      uint8_t p = entropy.next8() % len;
      leds[p] = blendColor(leds[p], CRGB::White, 150);
    }
  }
};

/* ================================================================
 * RAINBOW — 5-LED port of the V1.8 rainbow
 * ================================================================ */
class ModeRainbow : public Mode {
  uint8_t hue = 0;
  uint8_t spd = 3;
  bool rev = false;

public:
  void enter() override { hue = entropy.next8(); }

  void update(Gesture g) override {
    if (g == G_TAP)    spd = (uint8_t)(spd % 10 + 1);
    if (g == G_DOUBLE) rev = !rev;
  }

  void render() override {
    uint8_t s = (uint8_t)(spd * (globalSpeed + 40) / 75);
    if (!s) s = 1;
    if (rev) hue -= s;
    else     hue += s;
    fill_rainbow(leds, NUM_LEDS, hue, 16);
  }
};

/* ================================================================
 * MODE INSTANCES
 * ================================================================ */
ModeStatus  modeStatus;
ModeFire    modeFire;
ModeSaber   modeSaber;
ModeRainbow modeRainbow;

Mode* mainModes[] = {
  &modeStatus,
  &modeFire,
  &modeSaber,
  &modeRainbow
};
const uint8_t MAIN_MODE_COUNT = 4;

/* ================================================================
 * MENU HELPERS
 * ================================================================ */
void touchMenu() {
  menuLastActivity = millis();
}

bool inAnyMenu() {
  return uiState == UI_MENU_MAIN   ||
         uiState == UI_MENU_SETTINGS ||
         uiState == UI_BRIGHTNESS  ||
         uiState == UI_SPEED       ||
         uiState == UI_INFO ||
          uiState == UI_INVERTER;
}

/* ================================================================
 * MENU WINDOWING — six items, five LEDs
 * ================================================================ */
static uint8_t menuWindowBase(uint8_t cursor, uint8_t count) {
  if (count <= NUM_LEDS) return 0;
  if (cursor >= NUM_LEDS - 1) return cursor - (NUM_LEDS - 1);
  return 0;
}

static uint8_t menuCursorLed(uint8_t cursor, uint8_t base) {
  return cursor - base;
}

/* ================================================================
 * HOME MENU
 * ================================================================ */
void renderMainMenu() {
  clearLeds();
  if (inverterPower) {
    // D1 blue (status), D2 purple (inverter), D3 amber (settings).
    uint8_t base = menuWindowBase(mainCursor, 3);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      uint8_t item = base + i;
      if (item == 0) leds[i] = COLOR_PORTAL_BLUE;
      else if (item == 1) leds[i] = COLOR_PORTAL_PURPLE;
      else leds[i] = COLOR_PORTAL_AMBER;
    }
    renderCursor(menuCursorLed(mainCursor, base));
    return;
  }
  // 5V mode: status + the toy modes + settings, V1.8 colors.
  uint8_t base = menuWindowBase(mainCursor, 5);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t item = base + i;
    if (item < 4) leds[i] = COLOR_CACTUS;
    else leds[i] = COLOR_SETTINGS;
  }
  renderCursor(menuCursorLed(mainCursor, base));
}

/* ================================================================
 * SETTINGS MENU
 * ================================================================ */
const uint8_t SETTINGS_COUNT = 6;

void renderSettingsMenu() {
  clearLeds();
  uint8_t base = menuWindowBase(settingsCursor, SETTINGS_COUNT);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t item = base + i;
    if (item == 0) leds[i] = COLOR_SETTINGS;    // Back
    else if (item == 1) leds[i] = COLOR_CACTUS; // Brightness
    else if (item == 2) leds[i] = COLOR_CACTUS; // Speed
    else if (item == 3) leds[i] = COLOR_CACTUS; // Info
    else if (item == 4) leds[i] = CRGB(180, 120, 0); // Bootsel
    else if (item == 5) leds[i] = CRGB(255, 0, 0);   // Shutdown
  }
  renderCursor(menuCursorLed(settingsCursor, base));
}

/* ================================================================
 * BRIGHTNESS — 10 levels mapped to 5 LEDs
 * ================================================================ */
const uint8_t brightVals[10] = {6, 12, 20, 30, 42, 56, 72, 88, 106, 125};
uint8_t brightLevel = 0;

void renderBrightness() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = (brightLevel >= i * 2) ? COLOR_HOT_PINK : COLOR_CACTUS;
  }
}

/* ================================================================
 * SPEED — 10 levels mapped to 5 LEDs
 * ================================================================ */
const uint8_t speedVals[10] = {40, 55, 70, 85, 100, 120, 145, 170, 200, 240};
uint8_t speedLevel = 4;

void renderSpeed() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = (speedLevel >= i * 2) ? COLOR_HOT_PINK : COLOR_CACTUS;
  }
}

/* ================================================================
 * INFO
 * ================================================================ */
uint8_t infoPage = 0;

void renderInfo() {
  clearLeds();
  if (infoPage == 0) {
    fill_solid(leds, NUM_LEDS, COLOR_CACTUS);
    leds[0] = COLOR_HOT_PINK;
  } else if (infoPage == 1) {
    uint8_t n = (uint8_t)((millis() / 1000UL) % 6UL);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      leds[i] = (i < n) ? COLOR_CACTUS : CRGB::Black;
    }
  } else {
    fill_solid(leds, NUM_LEDS, COLOR_CACTUS);
  }
}

/* ================================================================
 * SHUTDOWN
 * ================================================================ */
bool sleepWaitRelease = true;
uint32_t sleepStart = 0;

void enterShutdown() {
  powerSetArmed(false);
  uiState = UI_SLEEP;
  sleepWaitRelease = true;
  sleepStart = 0;
  clearLeds();
  FastLED.setBrightness(0);
  FastLED.show();
}

void updateShutdown() {
  if (sleepWaitRelease) {
    if (digitalRead(BUTTON_PIN) == HIGH) sleepWaitRelease = false;
    return;
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!sleepStart) sleepStart = millis();
    if (millis() - sleepStart >= 1500) {
      sleepStart = 0;
      applyBrightness();
      uiState = UI_MODE;
      currentMainMode = 0;
      mainCursor = 0;
      modeStatus.enter();
      sleepWaitRelease = true;
    }
  } else {
    sleepStart = 0;
  }
}

/* ================================================================
 * BOOTSEL
 * ================================================================ */
void enterBootsel() {
  powerSetArmed(false);
  clearLeds();
  FastLED.setBrightness(0);
  FastLED.show();
  delay(40);
  // The hardware watchdog is still running.  reset_usb_boot() never returns,
  // so without this the watchdog would reset the chip ~500 ms later, kicking
  // it out of BOOTSEL mode.
  watchdog_disable();
  reset_usb_boot(0, 0);
  while (true) delay(1000);
}

/* ================================================================
 * RETURN TO DEFAULT IDLE
 * ================================================================ */
void returnToIdle() {
  // Idle means disarmed.  The UI's resting state must never leave the
  // inverter able to start on its own after the portal or a menu is left,
  // and a long hold is one of the documented ways to stop it.
  powerSetArmed(false);
  uiState = UI_MODE;
  currentMainMode = 0;
  mainCursor = 0;
  modeStatus.enter();
}

/* ================================================================
 * UI HANDLER
 * ================================================================ */
void handleUI(Gesture g) {
  if (g == G_NONE) return;
  if (inAnyMenu()) touchMenu();

  // Very-long → force idle
  if (g == G_VERY_LONG) {
    powerSetArmed(false);   // stop the bridge before any blocking delay
    fill_solid(leds, NUM_LEDS, COLOR_HOT_PINK);
    FastLED.show();
    delay(120);
    returnToIdle();
    return;
  }

  // -------- NORMAL MODE --------
  if (uiState == UI_MODE) {
    if (g == G_HOLD) {
      mainCursor = currentMainMode;
      uiState = UI_MENU_MAIN;
      touchMenu();
      return;
    }
    if (g == G_LONG_HOLD) {
      returnToIdle();
      return;
    }
    mainModes[currentMainMode]->update(g);
    return;
  }

  // -------- HOME MENU --------
  // Inverter mode: Status / Inverter / Settings.  5V mode: Status /
  // Fireplace / Lightsaber / Rainbow / Settings.
  if (uiState == UI_MENU_MAIN) {
    uint8_t homeItems = inverterPower ? 3 : 5;
    if (g == G_TAP) {
      mainCursor = (uint8_t)((mainCursor + 1) % homeItems);
      return;
    }
    if (g == G_DOUBLE) {
      mainCursor = (mainCursor == 0) ? homeItems - 1 : mainCursor - 1;
      return;
    }
    if (g == G_HOLD) {
      switch (mainCursor) {
        case 0:
          currentMainMode = 0;
          modeStatus.enter();
          uiState = UI_MODE;
          return;
        case 1:
          if (inverterPower) {
            uiState = UI_INVERTER;
          } else {
            currentMainMode = 1;
            modeFire.enter();
            uiState = UI_MODE;
          }
          touchMenu();
          return;
        case 2:
          if (inverterPower) {
            uiState = UI_MENU_SETTINGS;
          } else {
            currentMainMode = 2;
            modeSaber.enter();
            uiState = UI_MODE;
          }
          touchMenu();
          return;
        case 3:
          currentMainMode = 3;
          modeRainbow.enter();
          uiState = UI_MODE;
          return;
        case 4:
          uiState = UI_MENU_SETTINGS;
          touchMenu();
          return;
      }
    }
    if (g == G_LONG_HOLD) {
      returnToIdle();
      return;
    }
  }

  // -------- SETTINGS MENU --------
  if (uiState == UI_MENU_SETTINGS) {
    if (g == G_TAP) {
      settingsCursor = (uint8_t)((settingsCursor + 1) % SETTINGS_COUNT);
      return;
    }
    if (g == G_DOUBLE) {
      settingsCursor = (settingsCursor == 0) ? SETTINGS_COUNT - 1 : settingsCursor - 1;
      return;
    }
    if (g == G_HOLD) {
      if (settingsCursor == 0) {
        uiState = UI_MENU_MAIN;
        mainCursor = inverterPower ? 2 : 4;
        touchMenu();
        return;
      }
      if (settingsCursor == 1) {
        settingsReturnCursor = 1;
        uint8_t best = 255;
        for (uint8_t i = 0; i < 10; i++) {
          uint8_t difference = (uint8_t)abs((int)brightVals[i] - (int)globalBrightness);
          if (difference < best) {
            best = difference;
            brightLevel = i;
          }
        }
        uiState = UI_BRIGHTNESS;
        touchMenu();
        return;
      }
      if (settingsCursor == 2) {
        settingsReturnCursor = 2;
        uint8_t best = 255;
        for (uint8_t i = 0; i < 10; i++) {
          uint8_t difference = (uint8_t)abs((int)speedVals[i] - (int)globalSpeed);
          if (difference < best) {
            best = difference;
            speedLevel = i;
          }
        }
        uiState = UI_SPEED;
        touchMenu();
        return;
      }
      if (settingsCursor == 3) {
        settingsReturnCursor = 3;
        infoPage = 0;
        uiState = UI_INFO;
        touchMenu();
        return;
      }
      if (settingsCursor == 4) {
        enterBootsel();
        return;
      }
      if (settingsCursor == 5) {
        enterShutdown();
        return;
      }
    }
    if (g == G_LONG_HOLD) {
      uiState = UI_MENU_MAIN;
      mainCursor = inverterPower ? 2 : 4;
      touchMenu();
      return;
    }
  }

  // -------- BRIGHTNESS --------
  if (uiState == UI_BRIGHTNESS) {
    if (g == G_TAP) {
      brightLevel = (uint8_t)((brightLevel + 1) % 10);
      globalBrightness = brightVals[brightLevel];
      applyBrightness();
      touchMenu();
      return;
    }
    if (g == G_DOUBLE) {
      brightLevel = (brightLevel == 0) ? 9 : brightLevel - 1;
      globalBrightness = brightVals[brightLevel];
      applyBrightness();
      touchMenu();
      return;
    }
    if (g == G_HOLD || g == G_LONG_HOLD) {
      uiState = UI_MENU_SETTINGS;
      settingsCursor = settingsReturnCursor;
      touchMenu();
      return;
    }
  }

  // -------- SPEED --------
  if (uiState == UI_SPEED) {
    if (g == G_TAP) {
      speedLevel = (uint8_t)((speedLevel + 1) % 10);
      globalSpeed = speedVals[speedLevel];
      touchMenu();
      return;
    }
    if (g == G_DOUBLE) {
      speedLevel = (speedLevel == 0) ? 9 : speedLevel - 1;
      globalSpeed = speedVals[speedLevel];
      touchMenu();
      return;
    }
    if (g == G_HOLD || g == G_LONG_HOLD) {
      uiState = UI_MENU_SETTINGS;
      settingsCursor = settingsReturnCursor;
      touchMenu();
      return;
    }
  }

  // -------- INFO --------
  if (uiState == UI_INFO) {
    if (g == G_TAP) {
      infoPage = (uint8_t)((infoPage + 1) % 3);
      touchMenu();
      return;
    }
    if (g == G_DOUBLE) {
      infoPage = (infoPage == 0) ? 2 : infoPage - 1;
      touchMenu();
      return;
    }
    if (g == G_HOLD || g == G_LONG_HOLD) {
      uiState = UI_MENU_SETTINGS;
      settingsCursor = settingsReturnCursor;
      touchMenu();
      return;
    }
  }

  if (uiState == UI_INVERTER) {
    if (g == G_TAP) {
      // No action: the portal is a status display.
      touchMenu();
      return;
    }
    if (g == G_DOUBLE) {
      if (powerFaulted()) powerClearFault();
      touchMenu();
      return;
    }
    if (g == G_HOLD) {
      if (powerFaulted()) {
        powerClearFault();
        touchMenu();
        return;
      }
      if (!powerSensorsOk()) {
        touchMenu();
        return;
      }
      powerSetArmed(!powerArmed());
      touchMenu();
      return;
    }
    if (g == G_LONG_HOLD) {
      powerSetArmed(false);   // leaving the portal stops the inverter
      uiState = UI_MENU_MAIN;
      mainCursor = 1;
      touchMenu();
      return;
    }
  }
}

/* ================================================================
 * RENDER UI
 * ================================================================ */
void renderUI() {
  switch (uiState) {
    case UI_MODE:          mainModes[currentMainMode]->render(); break;
    case UI_MENU_MAIN:     renderMainMenu();                     break;
    case UI_MENU_SETTINGS: renderSettingsMenu();                 break;
    case UI_BRIGHTNESS:    renderBrightness();                   break;
    case UI_SPEED:         renderSpeed();                        break;
    case UI_INFO:          renderInfo();                         break;
    case UI_INVERTER:      renderInverter();                    break;
    case UI_SLEEP:         clearLeds();                          break;
  }
}

/* ================================================================
 * POWER STAGE
 * ================================================================
 *
 *  BRING-UP WARNING
 *  ----------------
 *  The H-bridge on the main Lamoka1/2 board has never been energised.
 *  The dead-time between high-side and low-side transitions is created
 *  entirely by this firmware; there is no hardware dead-time generator.
 *  Before the bridge is first enabled with bus power, verify GPIO1–4 on
 *  a scope with the gate-driver inputs exposed: confirm that both inputs
 *  of each half-bridge never go high simultaneously and that the low-side
 *  pulse width is reduced by the dead-time gap on both edges.
 *
 *  Power topology (autonomous boost, firmware-controlled H-bridge)
 *  ---------------------------------------------------------------
 *  Battery 11–14 V → Rsense1 → LM5122 boost → 21.75 V bus → UCC27735
 *  gate drivers → Q3/Q4/Q5/Q6 H-bridge → 12 V winding of a Triad
 *  VPT24-6670 → 115 V winding → 120.0 V RMS outlet.
 *
 *  The transformer is a Triad VPT24-6670: 160 VA, 115 V primary,
 *  12.0 V @ 13.34 A secondary on the parallel connection, 8 % TYP
 *  regulation from full load to no load, 89 % efficiency at full load.
 *  The 12 V winding is DRIVEN by the bridge; the 115 V winding is the
 *  outlet.  Its nameplate 12.0 V is the FULL-LOAD secondary voltage, so
 *  the open-circuit secondary is 12.0 × 1.08 = 12.96 V and the true
 *  no-load turns ratio is 115/12.96 = 8.8735 — not 115/12, which is
 *  the full-load ratio and 8 % high.
 *
 *  ---------------------------------------------------------------------
 *  WARNING: nothing on this board senses the 120 V RMS side.
 *  ---------------------------------------------------------------------
 *  The monitor chain is the battery rail (AIN0), the bus (AIN1), the two
 *  bridge midpoints (AIN2/AIN3), the input current (AIN4) and two
 *  TMP235s.  There is NO voltage and NO current measurement after the
 *  transformer: nothing in this firmware can see what the 115 V winding
 *  is doing.  The outlet voltage is an ESTIMATE, and the only way to
 *  know the real outlet voltage is to measure it with a meter.
 *
 *  The estimate is good - it holds the outlet within about 3 % of
 *  120.0 V RMS for a transformer regulating 6-10 % - but it is still an
 *  estimate.  Proceed with care: treat the output as live mains, verify
 *  it with a meter, and do not expect the firmware to notice a fault on
 *  the 115 V side.
 *
 *  A later Lamoka1 model may add a direct isolated output sense (an
 *  AMC1311B) and replace the estimate with a measurement.  This
 *  prototype does not have one.  Until it does, the 125 V receptacle
 *  ceiling is enforced on the estimate.
 *
 *  How the estimate is built:
 *
 *    1. measures the DRIVE winding (12 V) RMS from the bridge midpoint
 *       differential (AIN2 - AIN3);
 *    2. estimates the output power from the battery-side power
 *       (AIN0 x AIN4), less the auxiliary load, times an efficiency
 *       constant;
 *    3. estimates the OUTLET from the drive and the load with the
 *       transformer's series impedance (droop compensation):
 *
 *           V_out_est = A_DROOP * V_drive_rms - B_DROOP * I_sec_est
 *
 *       solved in closed form (see outletEstimate());
 *    4. closes the loop on V_out_est.  The compensation is a feed
 *       forward, so a small error in B_DROOP shows up as a small
 *       steady-state error at load, not as instability.
 *
 *  U5/U6 VDD and the bootstrap diodes run from CSP (the 11–14 V battery
 *  rail), not the 21.75 V bus.  The MCU only generates PWM; the LM5122
 *  runs in hardware.  The 21.75 V bus is set by the LM5122's 1.2 V
 *  feedback reference: 1.2 × (1 + Rfbt1/Rfbb1) = 21.749 V, i.e. Rfbt1 =
 *  64.9 kΩ with Rfbb1 = 3.79 kΩ.
 *
 *  Safety rules
 *  ------------
 *  - All four gate-driver inputs are driven low as the very first thing
 *    in setup() and are parked low whenever the bridge is not running.
 *  - The bridge is never enabled before the user explicitly arms it (the
 *    Inverter portal in the UI).  A valid voltage reading alone is not
 *    proof the hardware is safe to switch.
 *  - The bridge is never enabled before AIN1 confirms the 21.75 V bus.
 *  - Dead time is ≥ 500 ns on both edges by construction (phase-offset
 *    slices plus delayed inverted-channel compare).  The sine is written
 *    from the 20 kHz carrier-wrap ISR, once per period, so loop jitter
 *    cannot make the two slices latch different duty values.
 *  - Any fault latches POWER_FAULT and parks the gates low.  Non-critical
 *    faults can be cleared from the Inverter portal (double tap or hold);
 *    over-current and over-temperature require an MCU reset.  Clearing
 *    disarms the bridge.
 *  - Sleep mode and BOOTSEL entry disarm the bridge before acting.
 *  - A hardware watchdog (500 ms) is fed from the main loop and during the
 *    boot sweep.  If the MCU resets, the UCC27735 input pull-downs bring
 *    the gates low.
 *
 *  PWM dead-time scheme (from brief, kept in comments)
 *  ---------------------------------------------------
 *  GPIO1 (leg A high) → PWM slice 0 channel B, non-inverted, CC = dA*T
 *  GPIO2 (leg A low)  → PWM slice 1 channel A, inverted,    CC = dA*T + 2*dt
 *  GPIO3 (leg B high) → PWM slice 1 channel B, non-inverted, CC = dB*T
 *  GPIO4 (leg B low)  → PWM slice 2 channel A, inverted,    CC = dB*T + 2*dt
 *
 *  All three slices share TOP = T-1, divider = 1 (150 MHz PWM clock),
 *  up-counting mode.  Before enabling, counters are phased so the low-side
 *  slices lead by dt and 2dt respectively:
 *    slice 0 CTR = 0
 *    slice 1 CTR = +dt
 *    slice 2 CTR = +2*dt
 *  Combined with the delayed inverted-channel compares, this gives each leg
 *  a dt gap on both edges for any duty d in [2*dt/T, 1 - 2*dt/T], so we
 *  clamp |m| ≤ 1 - 4*dt/T.
 *
 *  WHAT THE WINDING ACTUALLY SEES
 *  ------------------------------
 *  The two legs are NOT 180 degrees apart.  Slice 1 carries leg A's low
 *  side AND leg B's high side, and the three counters are only dt and 2dt
 *  apart, so the legs are nearly in phase.  The differential across the
 *  primary is therefore a one-sided 20 kHz pulse train whose average is
 *  m * Vbus -- not a bipolar 20 kHz square wave.  That average is the 60 Hz
 *  fundamental the regulation model uses (drive RMS = m * Vbus / sqrt(2)),
 *  and the firmware measures it correctly because the 339 Hz filter on the
 *  two midpoints removes the carrier before the ADC sees it.
 *
 *  What the transformer and the outlet see is the whole waveform: about
 *  1.16 times the fundamental in RMS at the design point (16.8 V total
 *  against 14.5 V fundamental at m = 0.941).  A later board
 *  that puts each leg on its own slice
 *  (leg A on slice 1 = GPIO2/GPIO3, leg B on slice 2 = GPIO4/GPIO5, with
 *  the slices 180 degrees apart) would give a true bipolar drive; that is
 *  a pin-assignment change, not a firmware change.
 * ================================================================ */

// PWM constants.  150 MHz / 7500 = 20 kHz carrier; dt = 75 ticks = 500 ns.
// The dead time and the 60 Hz phase increment are integer constants, so the
// carrier frequency and the output frequency are only correct at 150 MHz.
// Fail the build rather than silently produce 50 Hz or 80 Hz from a board
// menu with a different CPU frequency.
static_assert(F_CPU == 150000000UL,
              "PIE V1.9 PWM timing assumes a 150 MHz clock; "
              "do not change the board's CPU frequency");
#define PWM_TOP            7499
#define PWM_DT_TICKS       75
#define PWM_CARRIER_HZ     20000UL
#define PWM_M_MAX          ((float)(PWM_TOP + 1 - 4 * PWM_DT_TICKS - 2) / (float)(PWM_TOP + 1))

// Pin → slice/channel mapping (fixed by RP2350 GPIO multiplexer).
#define GATE_A_HI_PIN      1
#define GATE_A_LO_PIN      2
#define GATE_B_HI_PIN      3
#define GATE_B_LO_PIN      4
#define PWM_SLICE_A_HI     0
#define PWM_SLICE_A_LO     1
#define PWM_SLICE_B_HI     1
#define PWM_SLICE_B_LO     2
#define PWM_CHAN_A_HI      PWM_CHAN_B
#define PWM_CHAN_A_LO      PWM_CHAN_A
#define PWM_CHAN_B_HI      PWM_CHAN_B
#define PWM_CHAN_B_LO      PWM_CHAN_A

// ADS7128 I2C address.  ADDR pin is unconnected (datasheet DNP/DNP row) → 0x10.
#define ADS7128_ADDR       0x10
#define ADS7128_SYSTEM_STATUS  0x00
#define ADS7128_GENERAL_CFG    0x01
#define ADS7128_DATA_CFG       0x02
#define ADS7128_CHANNEL_SEL    0x11
#define ADS7128_RECENT_CH0_LSB 0xA0

// ADS7128 command opcodes (datasheet SBAS868A Table 9): every I2C access is
// opcode-framed — the first data byte of a frame is the opcode, then the
// register address, then data where applicable.
#define ADS7128_OP_WRITE       0x08   // single register write
#define ADS7128_OP_READ        0x10   // single register read
#define ADS7128_OP_SET_BITS    0x18   // set masked bits only
#define ADS7128_OP_BLK_READ    0x30   // read a continuous register block

// SYSTEM_STATUS bit positions (datasheet Table 12).
#define ADS7128_OSR_DONE       0x08   // bit 3: conversion/averaging complete

// How long the driver waits for OSR_DONE before it stops believing in it.
// The conversion itself takes about 2 us at the internal oscillator, so
// 200 us is a hundred times the conversion time and still well inside the
// 200 us sweep throttle -- the sweep rate does not change.
#define ADS7128_OSR_TIMEOUT_US 200UL

// Divider ratios (bottom / (top + bottom)) from the schematic.
// R27 (AIN1), R29 and R31 (AIN2/AIN3) are all 68 kΩ, so every one of them
// has a 25.74 V full scale.  45.3 kΩ on the midpoints would clip at
// 18.25 V and 43.2 kΩ on the bus at 17.56 V — both below the 21.75 V bus,
// which is why they are 68 kΩ.  The bus now sits at 2.79 V, well below the
// 3.3 V clip, and the ±0.90 V start and ±1.5 V run windows stay inside that
// headroom.
#define DIV_AIN0           (10.0f / (36.5f + 10.0f))   // input rail 11–14 V
#define DIV_AIN1           (10.0f / (68.0f + 10.0f))   // 21.75 V bus, 25.74 V FS
#define DIV_AIN2           (10.0f / (68.0f + 10.0f))   // bridge midpoint A
#define DIV_AIN3           (10.0f / (68.0f + 10.0f))   // bridge midpoint B
// AIN4 is the input current at 0.1 V/A: INA240 gain times the RCS1 shunt
// (as-built A3/100 x 1 mOhm; design A2/50 x 2 mOhm - same 0.1 V/A).  The
// INA240's reference pins are grounded, so its output cannot go below
// ground: a REVERSE current through RCS1 reads as 0 A and is invisible
// to the over-current trip.  The input-rail check still refuses to arm
// with a reversed battery, but the current direction itself is not
// observable.
#define INA240_V_PER_A     0.1f                         // AIN4, 0.1 V/A

// AIN2/AIN3 filter (recorded, not corrected).
//
// C29 and C30 (47 nF) sit ACROSS the 10 kΩ bottom resistor of each
// divider, so the corner is
//
//     f = 1 / (2*pi*10k*47n) = 339 Hz
//
// — NOT the 74.8 Hz that a 47 nF part in series with the divider would
// give.  At the 60 Hz output that is a 1.5 % gain error (the divider
// reads 1.5 % low) and a 10° lag on the measured drive voltage.
//
// The error is NOT corrected in firmware.  Both dividers are identical,
// so the error and the lag are common to AIN2 and AIN3 and largely
// cancel in the AIN2 − AIN3 difference the drive RMS is computed from;
// what is left is not exactly predictable (the divider's source
// impedance, the ADS7128's input impedance and the sampling all
// perturb it).  At 1.5 % it sits inside the ±3 % outlet bound below,
// so it is left alone rather than corrected.

// Transformer: Triad VPT24-6670 — 160 VA, 115 V primary, 12.0 V @ 13.34 A
// secondary on the parallel connection, 8 % TYP regulation from full load
// to no load, 89 % efficiency at full load.
//
// The nameplate 12.0 V is the FULL-LOAD secondary voltage, so the
// open-circuit (no-load) secondary is 12.0 × 1.08 = 12.96 V and the true
// no-load turns ratio is
//
//     k = 115 / 12.96 = 8.8735
//
// 115/12 = 9.5833 is the FULL-LOAD ratio; using it over-states the outlet
// by 8 % (the firmware would believe the outlet is at 120 V when it is
// really at 111 V).  Every outlet number in this firmware is referred
// to the NO-LOAD (open-circuit) value.
#define TRANSFORMER_NOLOAD_RATIO (115.0f / 12.96f)

// Droop compensation.  The transformer's series impedance makes the outlet
// sag with load, so the outlet cannot be inferred from the drive voltage
// alone: the no-load ratio over-states the loaded outlet by the droop
// (8 % regulation is about 4.7 V at 100 W).  The firmware therefore
// estimates the outlet as
//
//     V_out_est = A_DROOP * V_drive_rms - B_DROOP * I_sec_est
//
// with V_drive_rms the 12 V winding RMS (measured at the bridge
// midpoints) and I_sec_est the 115 V winding current.  A_DROOP is the
// no-load ratio; B_DROOP is the series impedance referred to the 115 V
// secondary, in ohms:
//
//     Z1 = (12.96 - 12.0) / 13.34 * k^2 = 5.666 Ω
//
// Both constants are the transformer's CATALOGUE values, used as they come.
// Nobody has to fit them, and nobody has to calibrate a unit.
//
// How far off can that be?  The one thing the model does not know is the
// unit's own regulation r: the catalogue says 8 % TYP with no published
// maximum.  For a transformer regulating r, the no-load secondary is
// 12.0 * (1 + r) V, the no-load ratio is k = 115 / (12.0 * (1 + r)) and the
// series impedance referred to the 115 V winding is
// Z = (12.0 * r / 13.34) * k^2.  The firmware asks for the drive that
// satisfies the model at 120.0 V, and the real outlet is then
// k * V_drive - Z * I_sec:
//
//     r      k        Z        0 W      25 W      50 W      75 W     100 W
//     6 %  9.0409   4.412   122.26   122.55   122.83   123.11   123.40
//     7 %  8.9564   5.051   121.12   121.26   121.40   121.54   121.68
//     8 %  8.8735   5.666   120.00   120.00   120.00   120.00   120.00
//     9 %  8.7920   6.258   118.90   118.76   118.63   118.50   118.36
//    10 %  8.7121   6.828   117.82   117.55   117.29   117.03   116.76
//    11 %  8.6336   7.376   116.76   116.37   115.98   115.59   115.20
//
// So the catalogue constants hold the outlet within 3 % of 120.0 V RMS
// across the whole load range for any transformer regulating between 6 %
// and 10 %, which brackets the 8 % TYP value and covers the spread between
// units out of the same box.  At 11 % the error reaches -4 %.
//
// The estimate of the output POWER is about 12 % high: ETA_BUCK_HBRIDGE
// assumes 0.92 for the whole chain where the transformer alone is 0.89.
// That error is proportional to the load, so it lands in the same place as
// the droop error and needs no separate correction.  It is NOT absorbed by
// the "load too heavy" limit, which is compared against the same estimate
// -- see LOAD_LIMIT_W.
//
// This is the accuracy the 120.0 V target is actually held to, because there
// is no isolated output measurement for the loop to close on.  If a unit
// reads outside roughly 116-124 V RMS, the transformer wiring and the boost
// bus are the things to check -- not the constants.

#define A_DROOP            TRANSFORMER_NOLOAD_RATIO
#define B_DROOP            5.666f              // secondary-referred series impedance

// Load estimate.  The MCU has no direct outlet measurement, so the load is
// inferred from the battery-side power and an efficiency constant.
#define P_AUX_WATTS        3.0f     // 5 V buck, MCU, LEDs and fans
#define ETA_BUCK_HBRIDGE   0.92f    // battery -> bus -> bridge -> transformer

// sqrt(2), used to convert between the bridge fundamental RMS and peak.
#define SQRT2              1.41421356f

// Output regulation.  The regulator drives the ESTIMATED OUTLET (see the
// droop compensation above) to 120.0 V RMS.
//
// The modulation needed is m = x * sqrt(2) / (A_DROOP * Vbus), with
// x = OUTPUT_TARGET_RMS + B_DROOP * I_sec the drive the transformer needs
// to hold the target at the load:
//   120 V at no load on the 21.75 V bus needs m = 0.879;
//   120 V at 50 W needs m = 0.897;
//   120 V at 100 W needs m = 0.914;
//   at the bottom of the bus band (21.13 V) 100 W needs m = 0.941, so
//   there is 1.97 % of modulation left against the 0.9597 clamp — and
//   that is only for a transformer regulating 8 %.  A unit
//   that regulates 10 % needs m = 0.966 at that corner, which the clamp
//   rejects: the "output out of reach" fault below reports it.
//   The no-load ceiling at the PWM clamp is 131 V, so the outlet ceiling
//   is enforced separately by OUTLET_V_MAX below.
#define OUTPUT_TARGET_RMS  120.0f
#define OUTPUT_OV_FACTOR   1.05f    // 126 V on the estimated outlet
#define OUTPUT_OV_TICKS    2        // 20 ms at the 100 Hz control loop
#define OUTLET_V_MAX       125.0f   // NEMA 5-15R receptacle rating
#define OUT_MISSING_FRACTION 0.60f  // bridge output vs commanded modulation
#define AMP_SLEW_MAX       0.010f   // per 10 ms tick, below dead-time step

// Primary DC offset.
//
// The trip threshold is 0.5 V and the trip is evaluated on every new
// least-squares DC estimate, NOT once per control tick.  Both follow from the
// transformer's flux budget, which puts the saturation knee at 11-13 ms for a
// 1 V offset at full modulation -- a flux budget of about 0.011 V.s.
//
// The estimate is the intercept of the per-channel fit described above, not
// the mean of a window of held midpoint values.  That distinction matters:
// AIN2 and AIN3 are sampled at different instants, so differencing the held
// values produces a staircase, and the 60 Hz fundamental aliases against the
// sweep.  Depending on the sweep period the mean of that staircase can leak
// 0.3-3 V of false DC into the estimate -- above the trip threshold.  The
// fit removes the fundamental exactly and does not have that failure mode.
//
// The fit is still a windowed estimate: after a step of X volts it only
// reaches the 0.5 V threshold once about 0.5/X of the 16.667 ms window has
// been filled with the step.  With the weighted ADS sweep a new estimate
// arrives about every millisecond (the two midpoints alternate; each one is
// sampled about every 2.1 ms in the host model), so the trip is evaluated
// roughly every millisecond while the bridge is running.  Two consecutive
// estimates above the threshold are required, which costs about one more
// estimate interval.  That is
// fast enough at the realistic offsets:
//
//   X = 0.5 V   window 16.7 ms   knee 22.0 ms   (caught during soft start)
//   X = 1.0 V   window  8.3 ms   knee 11.0 ms
//   X = 2.0 V   window  4.2 ms   knee  5.5 ms
//
// A 5 V or 10 V step saturates in 2.2 ms or 1.1 ms, which is faster than any
// windowed 60 Hz estimate can resolve; those are supervisory-only and the
// hardware has to survive them.  The same is true of an offset already
// present when the bridge starts: it is caught during the soft start, where
// the modulation (and therefore the AC flux) is still low and the offset has
// more flux margin, not at full modulation.
//
// The fit's DC noise is about 0.1 V at full modulation (the 20 kHz carrier
// aliases into the 339 Hz midpoint filters), so the 0.5 V threshold sits
// about 5x above the noise.  Two consecutive estimates make a false trip
// unlikely without slowing the realistic cases.
#define DC_TRIP_V          0.5f
#define DC_TRIP_SAMPLES    2

// "Output out of reach": the regulator is pinned at the PWM clamp and the
// estimated outlet is still below the target, so the target cannot be
// reached at this load with this transformer.  It is debounced over
// OUT_OF_REACH_TICKS control ticks (200 ms at the 100 Hz loop) because
// the integrator needs about 100 ms to walk into the clamp after a load
// step, and the outlet estimate is noisy.
#define OUT_OF_REACH_TICKS  20
#define OUT_OF_REACH_BAND_V 0.5f    // V below the target that counts

// The transformer's power capability.  The VPT24-6670 is 160 VA, so at
// the 120 V target it passes 160/120 = 1.333 A on the 115 V winding and
// 13.34 A on the 12 V winding.
//
// The drive is NOT a 60 Hz sine: the bridge is a 20 kHz unipolar PWM with
// the 60 Hz fundamental riding on it, so the primary winding sees a
// waveform whose total RMS is about 1.16 times its 60 Hz fundamental at the
// design point (16.8 V total against 14.5 V fundamental at m = 0.941).
// Per carrier the differential is +Vbus for a fraction m of the period, so
// the cycle mean square is Vbus^2 * mean|m| = Vbus^2 * 2A/pi and the ratio
// is 2/sqrt(pi*A).  What the transformer is rated on -- the winding
// currents -- is set by the load and stays sinusoidal: at 100 W the primary
// carries 6.9 A RMS of its 13.34 A rating (52 %) and the secondary 0.83 A
// of its 1.333 A (62 %).  The extra voltage is 20 kHz carrier, which adds
// core loss but not flux: the carrier's volt-seconds balance over every
// carrier period, so the core does not walk up.
//
// The product rating is 100 W continuous.  The firmware's limit is 150 W of
// ESTIMATED output power: the estimate runs about 12 % high
// (ETA_BUCK_HBRIDGE assumes 0.92 for the whole chain where the
// transformer alone is 0.89), so that is about 135 W at the outlet -- 85 %
// of the 115 V winding's current rating and 70 % of the 12 V winding's.
// Above that the transformer, not the modulator, is the limit.
#define LOAD_LIMIT_W        150.0f
#define LOAD_HEAVY_TICKS    50      // 500 ms debounce

// State-machine thresholds.
#define VIN_LOW            11.0f
#define VIN_HIGH           14.0f
#define VIN_HYST           0.2f
// 21.75 V boost bus.  The LM5122's 1.2 V feedback reference with
// Rfbt1 = 64.9 kΩ and Rfbb1 = 3.79 kΩ sets it:
// 1.2 x (1 + 64.9/3.79) = 21.749 V.  With 1 % resistors and a
// 1.188/1.2/1.212 V reference the tolerance band is 21.13 V to 22.39 V,
// i.e. ±0.63 V about the nominal, so the start window is ±0.90 V — it has
// to accept the whole band plus the reference tolerance.  The run window
// is ±1.5 V, which also has to absorb the load-dump droop and the 120 Hz
// bus ripple.  Both are inside the 25.74 V full scale of the 68 kΩ AIN1
// divider.
//
// Note the run window is WIDER than the modulation budget allows at full
// load: 120 V at 100 W needs m = 0.941 at the bottom of the bus band but
// m = 0.982 at the bottom of the run window (20.25 V), which the clamp
// rejects.  The "output out of reach" fault is what reports that.
#define VBUS_NOMINAL       21.75f
#define VBUS_START_WINDOW  0.90f
#define VBUS_RUN_WINDOW    1.50f
#define BUS_RUN_GRACE_TICKS 3       // 30 ms of out-of-range bus tolerated
#define IIN_WARN           15.0f
#define IIN_TRIP           20.0f
#define IIN_TRIP_TICKS     2        // 20 ms: a single noisy INA240 sample must not trip
#define RAIL_TRIP_TICKS      3        // 30 ms: same for a noisy rail sample
#define TEMP_WARN_C        60.0f
#define TEMP_TRIP_C        85.0f
#define TEMP_TRIP_TICKS    3        // 30 ms: a real over-temp is not a spike
#define TEMP_VALID_MIN_C   -40.0f   // TMP235 specified range
#define TEMP_VALID_MAX_C   150.0f

// Control-loop timing.
#define CTRL_INTERVAL_MS   10     // ~100 Hz outer loop
#define RAMP_TIME_MS       1500   // soft-start ramp
#define M_START            0.30f  // modulation at the end of soft start
#define AMP_KI             0.0003f // integrator gain per 10 ms tick per volt of error

// RMS and DC measurement, per midpoint channel.
//
// AIN2 and AIN3 are sampled by the ADS7128 at different instants, and the
// samples are held until the next sweep.  Differencing the two held values
// therefore produces a staircase whose mean over a window is NOT the mean of
// the differential: the 60 Hz fundamental aliases against the sweep and can
// leave a DC-mean residual larger than the 0.5 V DC trip.  The firmware
// therefore fits each channel separately to
//
//     v(t) = DC + a*sin(theta) + b*cos(theta)
//
// over the last mains period, using the carrier ISR's 60 Hz phase at the
// instant the conversion started.  The fit removes the fundamental exactly,
// even when the samples are unevenly spaced, so the DC intercept is a true
// DC estimate rather than a staircase mean.  The differential fundamental is
// (aA-aB, bA-bB), whose RMS is sqrt(a^2+b^2)/sqrt(2) -- the same quantity the
// droop model calls the drive RMS.
//
// The ring holds per-channel samples, not one entry per loop pass.  At the
// weighted sweep a midpoint channel updates about every 2.1 ms (host model),
// so 32 entries cover roughly four mains periods; 16.667 ms is what the fit
// actually uses, and the extra entries are there so a late sample does not
// empty the window.
#define RMS_RING_SIZE      32
#define RMS_MIN_SAMPLES    4      // a 3-parameter fit needs at least four
#define RMS_MIN_SPAN_US    12500UL   // 3/4 of a mains period of phase span
#define RMS_WINDOW_US      16667UL   // one 60 Hz period (also 2 x 120 Hz)
#define RMS_STALE_US       5000UL    // a fit older than this is no longer valid
#define RMS_INVALID_TRIP_MS 500UL    // running with no valid estimate is a fault

enum PowerState : uint8_t {
  POWER_IDLE,
  POWER_WAIT_BUS,
  POWER_STARTING,
  POWER_RUNNING,
  POWER_FAULT
};

static PowerState pState      = POWER_IDLE;
static bool       pFault      = false;
static bool       pArmed      = false;
static bool       sensorsOk   = false;
static bool       adsSwept    = false;
static uint32_t   pFaultTime  = 0;
static char       pFaultReason[24];

static float vIn     = 0.0f;
static float vBus    = 0.0f;
static float vMidA   = 0.0f;
static float vMidB   = 0.0f;
static float iIn     = 0.0f;
static float tempCtrl = 0.0f;
static float tempInv  = 0.0f;
static bool  tempCtrlValid = false;
static bool  tempInvValid  = false;

static float modulation = 0.0f;    // amplitude (mRamp or mTarget)
static float mTarget    = 0.0f;    // amplitude loop output
static float mRamp      = 0.0f;    // ramped setpoint during start
static uint32_t rampStartMs = 0;
static uint32_t lastCtrlMs  = 0;

// Carrier-synchronous modulation.  The slice-0 wrap ISR advances the 60 Hz
// phase and writes all four compares once per 20 kHz carrier.  Updating from
// the main loop lets the two phase-offset slices latch different duty values
// and breach the 500 ns dead time when the loop is slow.
#define SINE_TABLE_SIZE   64
#define SINE_PHASE_INC    12884902UL   // 60 Hz / 20 kHz * 2^32
static volatile uint32_t sinePhase = 0;
static volatile float    modAmp    = 0.0f;
static volatile bool     bridgeModulating = false;
static uint8_t outMissingCount = 0;
static uint8_t dcFaultCount = 0;
// Silent-death detector state: last 12-bit code per midpoint channel and
// how many times in a row it repeated.  A chip whose ADC has stopped (but
// whose I2C still answers) returns the same code forever; with the bridge
// switching, a healthy midpoint channel moves >>1 LSB per millisecond, so
// three identical codes in a row while driving means the data is dead.
static uint16_t adsRepCode[2] = {0xFFFF, 0xFFFF};
static uint8_t  adsRepCount[2] = {0, 0};
static uint8_t outOvCount = 0;
static uint8_t iinOverCount = 0;
static uint8_t railBadTicks = 0;
static bool warnedCurrent = false;
static bool warnedTemp = false;
static uint8_t busBadTicks = 0;
static uint8_t tempCtrlOverCount = 0;
static uint8_t tempInvOverCount = 0;
static uint8_t tempInvalidCount = 0;
static uint32_t rmsInvalidSinceMs = 0;
static uint32_t lastProbeMs = 0;

// One midpoint sample, with the 60 Hz phase captured when its conversion
// started.  s and c are that phase's sine and cosine, so the fit below is a
// plain linear least-squares solve.
struct MidSample {
  uint32_t tUs;
  float v;
  float s;
  float c;
};

static MidSample midRingA[RMS_RING_SIZE];
static MidSample midRingB[RMS_RING_SIZE];
static uint8_t midIndexA = 0;
static uint8_t midIndexB = 0;
static uint8_t midCountA = 0;
static uint8_t midCountB = 0;
static bool    midDirty = false;
static uint32_t midLastSampleUs = 0;

static float   primaryRms = 0.0f;        // measured 12 V winding fundamental
static float   primaryDc = 0.0f;         // DC offset check
static float   inferredOutletRms = 0.0f; // drive * no-load ratio (bridge check)
static float   outletEstRms = 0.0f;      // droop-compensated OUTLET estimate
static float   secCurrentEst = 0.0f;     // estimated 115 V winding current
static bool    loadUnsupported = false;  // the drive cannot support the load
static bool    rmsValid = false;

// Regulator state kept for the serial report, so the bench can see the
// margin between the modulation that is commanded, the modulation the
// target needs and the modulation ceiling.  mReqNow is what the target
// needs at the present load and bus; driveLimitNow is the ceiling the
// regulator is held to; regSaturated says the regulator is pinned at that
// ceiling and atPwmClamp says the ceiling is the PWM clamp itself (as
// opposed to the 125 V receptacle ceiling, which binds at low load).
static float   mReqNow       = 0.0f;
static float   driveLimitNow = 0.0f;
static bool    regSaturated  = false;
static bool    atPwmClamp    = false;
static uint8_t outOfReachCount = 0;   // debounce for "output out of reach"
static uint8_t loadHeavyCount  = 0;  // debounce for "load too heavy"

// Measured RMS of the drive winding (the 12 V transformer primary).
static float drivePrimaryRms() {
  return primaryRms;
}

// Droop-compensated estimate of the OUTLET (the 115 V secondary), in RMS.
// This is what the regulator controls; it is an estimate, not a
// measurement — see the power-stage header comment.
static float outletEstimate() {
  return outletEstRms;
}

// ADS7128 scanning state.
#define ADS_CH_COUNT       5
#define ADS_SCHEDULE_LEN   15
// Weighted sweep: 12 of the 15 conversions are the two bridge midpoints,
// because they are the ones the DC and RMS estimate is built from.  The
// slow channels (rail, bus, current) are each read once per 15-step
// sequence -- about 12.6 ms in the host model (about 0.84 ms per conversion),
// which is far faster than any of them can move.  A plain five-channel round-robin
// gave the midpoints only 270 Hz and made the DC estimate need a full cycle
// of held values.
static const uint8_t kAdsSchedule[ADS_SCHEDULE_LEN] = {
  2, 3, 2, 3, 2, 3, 2, 3,
  0, 2, 3, 1, 2, 3, 4
};
static uint8_t  adsStep = 0;
static uint8_t  adsChannel = 0;
static uint8_t  adsSeenMask = 0;
static bool     adsPresent = false;
static bool     adsClockOk = false;   // running at 400 kHz; arming needs it
static uint32_t adsLastConvUs = 0;
static uint32_t adsConvStartUs = 0;
static uint32_t adsSamplePhase = 0;
static uint32_t adsSampleUs = 0;
static uint32_t adsChTime[ADS_CH_COUNT] = {0};
static bool     adsOsrWarned = false;   // OSR_DONE never asserted, reported once
static bool     adsSlowWarned = false;  // running on the 100 kHz fallback
// Freshness limit for one channel.  The weighted sweep refreshes each
// midpoint about every 2 ms and the slow channels once per ~13 ms (host model),
// so 50 ms means several complete sweeps were missed: the chip (or the bus) has stopped
// delivering rather than merely skipped a slot.
#define ADS_STALE_US 50000UL
// Input-rail reading age that still counts as "battery present" for the power
// mode.  Looser than ADS_STALE_US on purpose: a loop stall (the 120 ms
// very-long-hold flash, or the sleep pass) must not read as a lost battery
// and flip the mode.  Entry into inverter mode still needs a fresh sample.
#define RAIL_MODE_STALE_US 500000UL
enum AdsPhase : uint8_t { ADS_IDLE, ADS_WAITING, ADS_READING };
static AdsPhase adsPhase = ADS_IDLE;

// Serial reporting rate limit.
static uint32_t lastReportMs = 0;
static PowerState lastReportedState = POWER_IDLE;

// Helpers: clamp without min/max library calls.
static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void parkGatesLow() {
  pwm_set_mask_enabled(0);
  pinMode(GATE_A_HI_PIN, OUTPUT);
  pinMode(GATE_A_LO_PIN, OUTPUT);
  pinMode(GATE_B_HI_PIN, OUTPUT);
  pinMode(GATE_B_LO_PIN, OUTPUT);
  digitalWrite(GATE_A_HI_PIN, LOW);
  digitalWrite(GATE_A_LO_PIN, LOW);
  digitalWrite(GATE_B_HI_PIN, LOW);
  digitalWrite(GATE_B_LO_PIN, LOW);
}

static void setFault(const char *reason) {
  if (pFault) return;
  pFault = true;
  // A fault always leaves the bridge disarmed.  Keeping pArmed true while
  // faulted made the serial report say ARMED=Y and let a stale arm state
  // survive into the next arm attempt; the only safe invariant is
  // FAULT => not armed.
  pArmed = false;
  pState = POWER_FAULT;
  stopBridge();
  strncpy(pFaultReason, reason, sizeof(pFaultReason) - 1);
  pFaultReason[sizeof(pFaultReason) - 1] = '\0';
  pFaultTime = millis();
  if (Serial) {
    Serial.print("FAULT: ");
    Serial.println(reason);
  }
}

// Input-rail hysteresis state.  File scope (reset by stopBridge()) so a
// new run does not inherit the previous run's hysteresis side.  It is NOT
// cleared by startBridge(): the WAIT_BUS gate proves the rail live on the
// same tick, and a fresh un-hysteretic sample on the first powered tick
// would coin-flip a battery sitting exactly on an 11/14 V edge.
// Starts true: with no prior reading the wide window is the safe default —
// a truly bad rail (0 V, 20 V) fails even the wide window, while a strict
// first sample at exactly 11.0 V plus a millivolt of noise faults a good
// battery half the time.  The state self-corrects on the first evaluation.
static bool inputRailWasValid = true;

static bool inputRailValid() {
  bool wasValid = inputRailWasValid;
  bool ok = (vIn >= VIN_LOW - (wasValid ? VIN_HYST : 0.0f)) &&
            (vIn <= VIN_HIGH + (wasValid ? VIN_HYST : 0.0f));
  inputRailWasValid = ok;
  return ok;
}

static bool busValidForStart() {
  return (vBus >= VBUS_NOMINAL - VBUS_START_WINDOW) &&
         (vBus <= VBUS_NOMINAL + VBUS_START_WINDOW);
}

static bool busValidForRun() {
  // The bus can be disturbed by a load step and by the 120 Hz ripple, so a
  // single out-of-range sample is not a fault.  BUS_RUN_GRACE_TICKS
  // consecutive out-of-range control ticks (30 ms at the 100 Hz control
  // loop) are tolerated before the bridge is stopped; the tick after that
  // faults.  One tick of grace was not enough: a 20 ms excursion, which the
  // bus can produce on a load dump, used to stop the inverter.
  //
  // badTicks is file scope and is cleared by startBridge()/stopBridge().
  // As a function-static it survived a fault, so the first run after a clear
  // could fault on its first out-of-range tick with no grace at all.
  bool ok = (vBus >= VBUS_NOMINAL - VBUS_RUN_WINDOW) &&
            (vBus <= VBUS_NOMINAL + VBUS_RUN_WINDOW);
  if (ok) {
    busBadTicks = 0;
    return true;
  }
  if (busBadTicks < BUS_RUN_GRACE_TICKS) {
    busBadTicks++;
    return true;    // grace
  }
  return false;
}

// ADS7128 register primitives (opcode-framed, datasheet SBAS868A §8.5).
static bool adsWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADS7128_ADDR);
  Wire.write(ADS7128_OP_WRITE);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool adsSetBits(uint8_t reg, uint8_t mask) {
  // Sets only the masked bits, so starting a conversion (CNVST) cannot clear
  // the statistics-module enable (STATS_EN).
  Wire.beginTransmission(ADS7128_ADDR);
  Wire.write(ADS7128_OP_SET_BITS);
  Wire.write(reg);
  Wire.write(mask);
  return Wire.endTransmission() == 0;
}

static bool adsReadReg(uint8_t reg, uint8_t *dst) {
  Wire.beginTransmission(ADS7128_ADDR);
  Wire.write(ADS7128_OP_READ);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)ADS7128_ADDR, (uint8_t)1) != 1) return false;
  *dst = (uint8_t)Wire.read();
  return true;
}

static bool adsReadResult(uint8_t ch, uint16_t *code) {
  // Block read: a single-register-read frame repeats the same byte for every
  // extra clock (datasheet Figure 34), so the LSB/MSB pair needs opcode 0x30.
  uint8_t lsb, msb;
  uint8_t base = ADS7128_RECENT_CH0_LSB + 2 * ch;
  Wire.beginTransmission(ADS7128_ADDR);
  Wire.write(ADS7128_OP_BLK_READ);
  Wire.write(base);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)ADS7128_ADDR, (uint8_t)2) != 2) return false;
  lsb = (uint8_t)Wire.read();
  msb = (uint8_t)Wire.read();
  *code = (uint16_t)(((uint16_t)msb << 8) | lsb) >> 4;
  return true;
}

static bool adsProbe(bool fastClock) {
  uint8_t status = 0;
  uint8_t cfg = 0;
  adsClockOk = false;
  // A probe is also the recovery path: abandon any half-finished conversion
  // and force a fresh weighted sweep after a bus glitch.
  adsPhase = ADS_IDLE;
  adsConvStartUs = 0;
  adsStep = 0;
  adsChannel = kAdsSchedule[0];
  adsSeenMask = 0;
  adsSwept = false;
  adsOsrWarned = false;
  adsRepCode[0] = adsRepCode[1] = 0xFFFF;
  adsRepCount[0] = adsRepCount[1] = 0;
  for (uint8_t i = 0; i < ADS_CH_COUNT; i++) adsChTime[i] = 0;
  // Drop the measurement window.  Its samples were taken with the old
  // phase reference and channel state; a fresh run rebuilds it.
  midIndexA = midIndexB = 0;
  midCountA = midCountB = 0;
  midDirty = false;
  rmsValid = false;
  primaryDc = 0.0f;
  primaryRms = 0.0f;
  inferredOutletRms = 0.0f;
  if (!adsReadReg(ADS7128_SYSTEM_STATUS, &status)) return false;
  // Force DATA_CFG to not append status bits, so reads are pure 16-bit codes.
  if (!adsWriteReg(ADS7128_DATA_CFG, 0x00)) return false;
  // RECENT_CHx only records while the statistics module is enabled
  // (datasheet §8.3.6 and §8.3.14); set STATS_EN once and read it back.
  if (!adsSetBits(ADS7128_GENERAL_CFG, 0x20)) return false;
  if (!adsReadReg(ADS7128_GENERAL_CFG, &cfg)) return false;
  bool ok = (cfg & 0x20) != 0;
  // The chip answered.  Only a probe made at 400 kHz counts as a clock the
  // weighted sweep can run on; a 100 kHz chip is detectable but must not be
  // armed (see the sweep comment above).
  adsClockOk = ok && fastClock;
  return ok;
}

static float adsCodeToVolts(uint16_t code) {
  return (float)code * 3.3f / 4096.0f;
}

static void pushMidSample(MidSample *ring, uint8_t *index, uint8_t *count,
                          float v, uint32_t phase, uint32_t tUs) {
  MidSample &sample = ring[*index];
  sample.tUs = tUs;
  sample.v = v;
  sample.s = sineAt(phase);
  sample.c = sineAt(phase + 0x40000000UL);   // +90 degrees in 2^32 units
  *index = (uint8_t)((*index + 1) % RMS_RING_SIZE);
  if (*count < RMS_RING_SIZE) (*count)++;
}

static bool adsStartConversion(uint8_t ch) {
  if (!adsWriteReg(ADS7128_CHANNEL_SEL, ch & 0x0F)) return false;
  if (!adsSetBits(ADS7128_GENERAL_CFG, 0x08)) return false;  // CNVST; STATS_EN stays set
  adsConvStartUs = micros();
  adsSampleUs = adsConvStartUs;
  // The 60 Hz phase at the instant the conversion was requested -- or rather,
  // just after the two I2C writes that request it (~100-200 us later, i.e.
  // 2-4 degrees at 60 Hz).  That error is common to both midpoint channels,
  // so it cancels in the differential fit; capturing the phase before the
  // writes would cost a second read of a volatile the ISR is updating.
  adsSamplePhase = sinePhase;
  adsPhase = ADS_WAITING;
  return true;
}

// One failure path for every ADS7128 transaction.  Dropping the 400 kHz
// flag is deliberate: a chip that has stopped answering at 400 kHz is not a
// chip to arm the bridge on, even if the 100 kHz probe still succeeds.
static void adsFail(const char *reason) {
  adsPresent = false;
  adsClockOk = false;
  adsPhase = ADS_IDLE;
  adsSwept = false;
  adsSeenMask = 0;
  adsRepCode[0] = adsRepCode[1] = 0xFFFF;
  adsRepCount[0] = adsRepCount[1] = 0;
  if (pArmed) setFault(reason);
}

static void adsService() {
  if (!adsPresent) return;

  if (adsPhase == ADS_WAITING) {
    // Poll OSR_DONE (bit 3).  Typical conversion < 2 us.
    uint8_t status = 0;
    if (!adsReadReg(ADS7128_SYSTEM_STATUS, &status)) {
      adsFail("ADS7128 NACK");
      return;
    }
    if (status & ADS7128_OSR_DONE) {
      // OSR_DONE is a WRITE-1-TO-CLEAR bit (datasheet Table 12: "Clear this
      // bit by writing 1b to this bit"), so it is cleared with a plain
      // register write.  The "set bit" opcode (0x18) is documented to SET
      // bits (datasheet 8.5.2.2), which is not the same operation on a
      // write-1-to-clear bit.
      if (!adsWriteReg(ADS7128_SYSTEM_STATUS, ADS7128_OSR_DONE)) {
        adsFail("ADS7128 NACK");
        return;
      }
      adsPhase = ADS_READING;
    } else if (micros() - adsConvStartUs > ADS7128_OSR_TIMEOUT_US) {
      // OSR_DONE has not asserted.  With the averaging filter disabled
      // (OSR_CFG = 0, the reset value, which the datasheet defines as "No
      // averaging") the datasheet describes this bit as the AVERAGING
      // status, so it may never assert at all.  The conversion itself took
      // about 2 us, so the result is long since ready: read it, and report
      // the condition once instead of faulting.  A genuinely dead chip is
      // still caught -- the read below fails and raises "ADS7128 read".
      if (!adsOsrWarned) {
        adsOsrWarned = true;
        if (Serial) Serial.println("WARN: ADS7128 OSR_DONE did not assert");
      }
      adsPhase = ADS_READING;
    }
    return;
  }

  if (adsPhase == ADS_READING) {
    uint8_t ch = adsChannel;
    uint16_t code = 0;
    if (!adsReadResult(ch, &code)) {
      adsFail("ADS7128 read");
      return;
    }
    float volts = adsCodeToVolts(code);
    uint32_t nowUs = micros();
    if ((ch == 2 || ch == 3) &&
        (pState == POWER_STARTING || pState == POWER_RUNNING)) {
      // Silent-death detector, evaluated at sample time so a dead chip is
      // named before its stale samples can trip the DC-offset check with a
      // misleading reason.  Three identical 12-bit codes in a row on a
      // switching bridge is not a measurement; see the declaration above.
      uint8_t m = (uint8_t)(ch - 2);
      if (code == adsRepCode[m]) {
        if (++adsRepCount[m] >= 3) {
          adsFail("sensor stale");
          return;
        }
      } else {
        adsRepCode[m] = code;
        adsRepCount[m] = 0;
      }
    }
    switch (ch) {
      case 0: vIn   = volts / DIV_AIN0; break;
      case 1: vBus  = volts / DIV_AIN1; break;
      case 2:
        vMidA = volts / DIV_AIN2;
        pushMidSample(midRingA, &midIndexA, &midCountA,
                      vMidA, adsSamplePhase, adsSampleUs);
        midDirty = true;
        midLastSampleUs = nowUs;
        break;
      case 3:
        vMidB = volts / DIV_AIN3;
        pushMidSample(midRingB, &midIndexB, &midCountB,
                      vMidB, adsSamplePhase, adsSampleUs);
        midDirty = true;
        midLastSampleUs = nowUs;
        break;
      case 4: iIn   = volts / INA240_V_PER_A; break;
    }
    adsChTime[ch] = nowUs;
    adsSeenMask |= (uint8_t)(1u << ch);
    if (adsSeenMask == (uint8_t)((1u << ADS_CH_COUNT) - 1u)) adsSwept = true;
    adsStep = (uint8_t)((adsStep + 1) % ADS_SCHEDULE_LEN);
    adsPhase = ADS_IDLE;
    adsLastConvUs = nowUs;
  }

  if (adsPhase == ADS_IDLE) {
    // Throttle slightly to keep the loop responsive.  The weighted schedule
    // still gives each midpoint a sample about every 2 ms.
    if (micros() - adsLastConvUs >= 200) {
      adsChannel = kAdsSchedule[adsStep];
      if (!adsStartConversion(adsChannel)) {
        adsFail("ADS7128 NACK");
      }
    }
  }
}

// Freshness of one ADS channel, against the millisecond-scale sweep.
static bool adsFresh(uint8_t ch, uint32_t nowUs) {
  if (ch >= ADS_CH_COUNT || adsChTime[ch] == 0) return false;
  return (uint32_t)(nowUs - adsChTime[ch]) <= ADS_STALE_US;
}

static float readTempAdc(uint8_t adcInput) {
  // The TMP235 is an ANALOGUE sensor on this board: the netlist has its
  // VOUT pin (not an I2C device) on GPIO26/ADC0 and GPIO27/ADC1.  Its
  // transfer function is 10 mV/degC with 500 mV at 0 degC, so the reading
  // below is the datasheet's own equation.
  //
  // adc_read() is a single-shot conversion with a short busy-wait (96 ADC
  // clock cycles, about 0.8 us), so it is not a real stall in the power
  // path; the 20 kHz carrier runs from the ISR and is unaffected.
  adc_select_input(adcInput);
  uint16_t raw = adc_read();
  float volts = (float)raw * 3.3f / 4096.0f;
  return (volts - 0.5f) / 0.01f;
}

static bool tempPlausible(float t) {
  // The TMP235 is specified over -40..150 C, which is 0.1 V..2.0 V at its
  // output.  A reading outside that is not a temperature: it is an open or
  // shorted sensor, and treating it as a temperature would either hide a
  // real over-temp (a short reads cold) or invent one (an open reads hot).
  return (t >= TEMP_VALID_MIN_C) && (t <= TEMP_VALID_MAX_C);
}

static void readTemperatures() {
  // Sample each sensor occasionally; TMP235 is slow and thermal mass is large.
  static uint8_t tempPhase = 0;
  static uint32_t lastTempMs = 0;
  uint32_t now = millis();
  if (now - lastTempMs < 100) return;
  lastTempMs = now;
  if (tempPhase == 0) {
    tempCtrl = readTempAdc(0);
    tempCtrlValid = tempPlausible(tempCtrl);
  } else {
    tempInv = readTempAdc(1);
    tempInvValid = tempPlausible(tempInv);
  }
  tempPhase ^= 1;
}

// Least-squares fit of v = dc + a*sin(theta) + b*cos(theta) over the samples
// that fall inside the last mains period.  Returns ok=false when there are
// too few samples, when they do not span enough phase, or when the normal
// matrix is too close to singular to trust.
struct FundamentalFit {
  float dc;
  float a;
  float b;
  bool  ok;
};

static FundamentalFit fitMidRing(const MidSample *ring, uint8_t index,
                                 uint8_t count, uint32_t nowUs) {
  FundamentalFit fit = {0.0f, 0.0f, 0.0f, false};
  float n = 0.0f, ss = 0.0f, sc = 0.0f;
  float sss = 0.0f, scc = 0.0f, ssc = 0.0f;
  float sy = 0.0f, sys = 0.0f, syc = 0.0f;
  uint32_t newestUs = 0, oldestUs = 0;
  uint8_t used = 0;

  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = (uint8_t)((index + RMS_RING_SIZE - 1 - i) % RMS_RING_SIZE);
    uint32_t age = nowUs - ring[idx].tUs;
    if (age > RMS_WINDOW_US) break;
    float s = ring[idx].s;
    float c = ring[idx].c;
    float v = ring[idx].v;
    if (used == 0) newestUs = ring[idx].tUs;
    oldestUs = ring[idx].tUs;
    used++;
    n   += 1.0f;
    ss  += s;
    sc  += c;
    sss += s * s;
    scc += c * c;
    ssc += s * c;
    sy  += v;
    sys += v * s;
    syc += v * c;
  }

  if (used < RMS_MIN_SAMPLES) return fit;
  if ((uint32_t)(newestUs - oldestUs) < RMS_MIN_SPAN_US) return fit;

  // Cramer's rule on the 3x3 normal equations.  With phases spread over
  // most of a cycle the matrix is well conditioned; a collapsed set (all
  // samples at the same phase) gives a near-zero determinant and is
  // rejected rather than returning a wild DC.
  float det = n * (sss * scc - ssc * ssc)
            - ss * (ss * scc - ssc * sc)
            + sc * (ss * ssc - sss * sc);
  float scale = n * n * n;
  if (fabsf(det) < 1e-3f * scale) return fit;

  fit.dc = (sy * (sss * scc - ssc * ssc)
            - ss * (sys * scc - ssc * syc)
            + sc * (sys * ssc - sss * syc)) / det;
  fit.a  = (n * (sys * scc - ssc * syc)
            - sy * (ss * scc - ssc * sc)
            + sc * (ss * syc - sys * sc)) / det;
  fit.b  = (n * (sss * syc - ssc * sys)
            - ss * (ss * syc - sys * sc)
            + sy * (ss * ssc - sss * sc)) / det;
  fit.ok = true;
  return fit;
}

static void updateRmsAndDc() {
  if (!midDirty) {
    // A fit must not outlive its data.  The weighted sweep normally produces
    // a new midpoint sample (either channel) about every millisecond; if none has arrived for
    // RMS_STALE_US the last fit is no longer a measurement of the present
    // drive, so drop it.  Without this, rmsValid would stay true forever
    // after the ADS7128 stopped converting without NACKing.
    if (rmsValid &&
        (uint32_t)(micros() - midLastSampleUs) > RMS_STALE_US) {
      rmsValid = false;
      primaryDc = 0.0f;
      primaryRms = 0.0f;
      inferredOutletRms = 0.0f;
      dcFaultCount = 0;
    }
    return;
  }
  midDirty = false;

  // The fit is only meaningful while the bridge is driving: outside a run
  // sinePhase is frozen, so every sample has the same phase and the fit is
  // correctly rejected.  The window is also cleared at start and stop.
  if (!pArmed || (pState != POWER_STARTING && pState != POWER_RUNNING)) {
    rmsValid = false;
    primaryDc = 0.0f;
    primaryRms = 0.0f;
    inferredOutletRms = 0.0f;
    dcFaultCount = 0;
    return;
  }

  uint32_t nowUs = micros();
  FundamentalFit fa = fitMidRing(midRingA, midIndexA, midCountA, nowUs);
  FundamentalFit fb = fitMidRing(midRingB, midIndexB, midCountB, nowUs);
  if (!fa.ok || !fb.ok) {
    rmsValid = false;
    primaryDc = 0.0f;
    primaryRms = 0.0f;
    inferredOutletRms = 0.0f;
    dcFaultCount = 0;
    return;
  }

  rmsValid = true;

  // Differential DC and fundamental.  Each channel's fit has already
  // removed its own 60 Hz term, so this is not the mean of a staircase.
  primaryDc = fa.dc - fb.dc;
  float a = fa.a - fb.a;
  float b = fa.b - fb.b;
  primaryRms = sqrtf(a * a + b * b) * 0.70710678f;   // 1/sqrt(2)

  // The NO-LOAD (open-circuit) ratio applied to the drive.  This
  // OVER-estimates the outlet under load (that is what the droop
  // compensation corrects), so it is used for the bridge plausibility
  // check only.  It is the same no-load ratio the droop model uses, so
  // the two never disagree about what the transformer does at no load.
  inferredOutletRms = primaryRms * TRANSFORMER_NOLOAD_RATIO;

  // The DC-offset trip lives here, not in the control loop.  It is
  // evaluated on every new fit, which the weighted sweep produces about
  // every millisecond while the bridge is running.  See DC_TRIP_V for why the
  // estimate is a fit rather than a window mean, and for the latency this
  // actually achieves.
  //
  // While a code repeat is pending on either midpoint (see the silent-death
  // detector above), the trip stands down and its counter resets: frozen
  // data stamped with fresh phases looks like an offset to a fit, and a
  // judgement made on dead data would only misname the fault.
  bool dcSuspect = (adsRepCount[0] >= 1) || (adsRepCount[1] >= 1);
  if (!dcSuspect && fabsf(primaryDc) > DC_TRIP_V) {
    if (++dcFaultCount >= DC_TRIP_SAMPLES) {
      setFault("DC offset");
      return;
    }
  } else {
    dcFaultCount = 0;
  }
}

/* ================================================================
 * OUTLET ESTIMATE
 * ================================================================
 *
 * The board has no isolated output sense, so the outlet cannot be
 * measured.  It is estimated from the measured drive winding and the
 * estimated load.
 */

// Estimated power reaching the outlet, in watts.  AIN4 measures the
// BATTERY current (through the INA240 and RCS1), so vIn * iIn is the
// power the battery supplies; the auxiliary load (5 V buck, MCU, LEDs,
// fans) comes off the top and the remainder is bus power, of which
// ETA_BUCK_HBRIDGE reaches the outlet.
static float outputPowerEstimate() {
  float pBatt = vIn * iIn;
  if (pBatt < 0.0f) pBatt = 0.0f;
  float pBus = pBatt - P_AUX_WATTS;
  if (pBus < 0.0f) pBus = 0.0f;
  return pBus * ETA_BUCK_HBRIDGE;
}

// Droop-compensated estimate of the outlet voltage.
//
// The droop model is V_out = A_DROOP * V_drive - B_DROOP * I_sec with
// I_sec = P_out / V_out, so V_out satisfies
//
//     V_out^2 - x * V_out + y = 0,   x = A_DROOP * V_drive,  y = B_DROOP * P_out
//
// which is solved in closed form — no iteration, no fixed-point loop:
//
//     V_out = (x + sqrt(x*x - 4*y)) / 2
//
// The physical root is the LARGER one.  The smaller root is the
// degenerate branch: at y = 0 it collapses to 0 V, which is not a
// no-load voltage, and under load it collapses to a few volts, which
// would make the regulator slam the modulation into the clamp.
//
// If x*x < 4*y the load is too heavy for this drive and the quadratic has
// no real root.  The load is then treated as zero (V_out = x) and
// loadUnsupported is raised.  That guard is a REPORTED condition only, not
// a fault trigger: at the end of the soft-start ramp the drive is still
// small, and the guard would trip on every load above about 74 W.  The
// faults that do trigger are the regulator's own "output out of reach"
// (the modulator cannot deliver the drive the target needs) and the
// "load too heavy" limit on the transformer's VA rating.
static void updateOutletEstimate() {
  loadUnsupported = false;

  float pOut = outputPowerEstimate();
  float x = A_DROOP * drivePrimaryRms();
  float y = B_DROOP * pOut;
  if (y < 0.0f) y = 0.0f;

  float v;
  if (y <= 0.0f) {
    v = x;                    // no load: the outlet is the no-load value
  } else if (x * x < 4.0f * y) {
    // The drive cannot support the load.  With no drive at all (the
    // bridge parked) this is not a condition, it is simply "off".
    loadUnsupported = (x > 0.0f);
    v = x;                    // treat the load as zero
  } else {
    v = 0.5f * (x + sqrtf(x * x - 4.0f * y));
  }
  if (v < 0.0f) v = 0.0f;
  outletEstRms = v;

  float iSec = pOut / ((v > 1.0f) ? v : 1.0f);
  if (iSec < 0.0f) iSec = 0.0f;
  secCurrentEst = iSec;
}

// The largest drive whose ESTIMATED outlet stays at or below
// OUTLET_V_MAX (the receptacle rating).  This is the hard ceiling that
// keeps the outlet inside 125 V even if the load drops away while the
// regulator is still over-driving.
//
// The estimate's own quadratic gives the drive: at V = OUTLET_V_MAX,
//   x = OUTLET_V_MAX + y / OUTLET_V_MAX
// and the modulation that produces a drive of x is
//   m = x * sqrt(2) / (A_DROOP * Vbus).
// At zero load (y = 0) this reduces to the plain no-load bound
// OUTLET_V_MAX * sqrt(2) / (A_DROOP * Vbus) — 0.916 on a 21.75 V bus.
//
// The limit is load-aware on purpose.  A no-load bound applied at every
// load would cap the drive BELOW what is needed to hold 120 V once the
// load passes about 106 W (the droop needs MORE drive at load, not less),
// so the outlet could not hold its target at the heavier end of its range.
static float outletDriveLimit(float y) {
  float bus = (vBus > 1.0f) ? vBus : VBUS_NOMINAL;
  float xMax = OUTLET_V_MAX + ((y > 0.0f) ? (y / OUTLET_V_MAX) : 0.0f);
  float mMax = xMax * SQRT2 / (A_DROOP * bus);
  return clampf(mMax, 0.0f, PWM_M_MAX);
}

static void __not_in_flash_func(writeBridgeDuty)(float m) {
  // m is the modulation index, clamped so duty stays inside dead-time band.
  m = clampf(m, -PWM_M_MAX, PWM_M_MAX);

  float dA = (1.0f + m) * 0.5f;
  float dB = (1.0f - m) * 0.5f;

  // Convert to PWM compare values (integer ticks).  Rounded, not truncated:
  // truncation shaves up to a tick off the programmed gap, leaving 493 ns
  // worst case against the 500 ns claim.
  uint16_t ccAhi = (uint16_t)(dA * (float)(PWM_TOP + 1) + 0.5f);
  uint16_t ccAlo = (uint16_t)(dA * (float)(PWM_TOP + 1) + 2.0f * (float)PWM_DT_TICKS + 0.5f);
  uint16_t ccBhi = (uint16_t)(dB * (float)(PWM_TOP + 1) + 0.5f);
  uint16_t ccBlo = (uint16_t)(dB * (float)(PWM_TOP + 1) + 2.0f * (float)PWM_DT_TICKS + 0.5f);

  if (ccAlo > PWM_TOP) ccAlo = PWM_TOP;
  if (ccBlo > PWM_TOP) ccBlo = PWM_TOP;
  if (ccAhi < 2 * PWM_DT_TICKS) ccAhi = 2 * PWM_DT_TICKS;
  if (ccBhi < 2 * PWM_DT_TICKS) ccBhi = 2 * PWM_DT_TICKS;

  pwm_set_chan_level(PWM_SLICE_A_HI, PWM_CHAN_A_HI, ccAhi);
  pwm_set_chan_level(PWM_SLICE_A_LO, PWM_CHAN_A_LO, ccAlo);
  pwm_set_chan_level(PWM_SLICE_B_HI, PWM_CHAN_B_HI, ccBhi);
  pwm_set_chan_level(PWM_SLICE_B_LO, PWM_CHAN_B_LO, ccBlo);
}

/* 60 Hz sine, 64 points plus the wrap entry.  The carrier ISR interpolates
 * between entries, so the per-carrier step follows the 60 Hz derivative
 * (~0.0189 at m=1) instead of the coarser table step, staying below the
 * 4*dt/T dead-time band. */
static const float kSineTable[SINE_TABLE_SIZE + 1] = {
  +0.000000000f, +0.098017140f, +0.195090322f, +0.290284677f, +0.382683432f, +0.471396737f, +0.555570233f, +0.634393284f,
  +0.707106781f, +0.773010453f, +0.831469612f, +0.881921264f, +0.923879533f, +0.956940336f, +0.980785280f, +0.995184727f,
  +1.000000000f, +0.995184727f, +0.980785280f, +0.956940336f, +0.923879533f, +0.881921264f, +0.831469612f, +0.773010453f,
  +0.707106781f, +0.634393284f, +0.555570233f, +0.471396737f, +0.382683432f, +0.290284677f, +0.195090322f, +0.098017140f,
  +0.000000000f, -0.098017140f, -0.195090322f, -0.290284677f, -0.382683432f, -0.471396737f, -0.555570233f, -0.634393284f,
  -0.707106781f, -0.773010453f, -0.831469612f, -0.881921264f, -0.923879533f, -0.956940336f, -0.980785280f, -0.995184727f,
  -1.000000000f, -0.995184727f, -0.980785280f, -0.956940336f, -0.923879533f, -0.881921264f, -0.831469612f, -0.773010453f,
  -0.707106781f, -0.634393284f, -0.555570233f, -0.471396737f, -0.382683432f, -0.290284677f, -0.195090322f, -0.098017140f,
  +0.000000000f,
};

// Look up the sine of a 2^32 phase, with the same linear interpolation the
// ISR uses.  The fit needs the phase at each ADS7128 conversion; cos is
// sineAt(phase + 2^30), i.e. +90 degrees.
static float sineAt(uint32_t phase) {
  uint32_t idx = (phase >> 26) & 0x3F;
  uint32_t frac = (phase >> 18) & 0xFF;
  float s0 = kSineTable[idx];
  float s1 = kSineTable[idx + 1];
  return s0 + (s1 - s0) * ((float)frac * (1.0f / 256.0f));
}

// Once per 20 kHz carrier, on the slice-0 wrap: advance 60 Hz phase and
// write all four compares.  Slice 0 wraps last in the three-slice sequence,
// so this leaves nearly a full carrier before the next slice wrap and all
// three slices latch the same value at their next wraps.
static void __not_in_flash_func(onPwmWrap)() {
  pwm_clear_irq(PWM_SLICE_A_HI);
  if (!bridgeModulating) return;
  sinePhase += SINE_PHASE_INC;
  uint32_t idx = (sinePhase >> 26) & 0x3F;
  uint32_t frac = (sinePhase >> 18) & 0xFF;
  float s0 = kSineTable[idx];
  float s1 = kSineTable[idx + 1];
  float s = s0 + (s1 - s0) * ((float)frac * (1.0f / 256.0f));
  writeBridgeDuty(modAmp * s);
}

static void resetMeasurementWindow() {
  // Drop both midpoint rings.  They hold samples from the previous run (or
  // the idle period before this one), and the filter's settling transient is
  // not a measurement of anything: keeping it would let the first fit of a
  // new run see a step and look like a DC offset.  The rings refill from the
  // new run.
  midIndexA = 0;
  midIndexB = 0;
  midCountA = 0;
  midCountB = 0;
  midDirty = false;
  midLastSampleUs = 0;
  rmsValid = false;
  primaryDc = 0.0f;
  primaryRms = 0.0f;
  inferredOutletRms = 0.0f;
}

static void stopBridge() {
  bridgeModulating = false;
  modAmp = 0.0f;
  parkGatesLow();
  modulation = 0.0f;
  mTarget = 0.0f;
  mRamp = 0.0f;
  outMissingCount = 0;
  dcFaultCount = 0;
  adsRepCode[0] = adsRepCode[1] = 0xFFFF;
  adsRepCount[0] = adsRepCount[1] = 0;
  outOvCount = 0;
  iinOverCount = 0;
  railBadTicks = 0;
  warnedCurrent = false;
  warnedTemp = false;
  outOfReachCount = 0;
  loadHeavyCount = 0;
  tempCtrlOverCount = 0;
  tempInvOverCount = 0;
  tempInvalidCount = 0;
  busBadTicks = 0;
  rmsInvalidSinceMs = 0;
  inputRailWasValid = true;
  mReqNow = 0.0f;
  driveLimitNow = 0.0f;
  regSaturated = false;
  atPwmClamp = false;
  resetMeasurementWindow();
}

static void enablePwmSlices() {
  // Configure all three slices identically, then phase the counters and
  // enable them with a single write so the offsets stay locked.
  for (uint8_t s = 0; s < 3; s++) {
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 1.0f);
    pwm_config_set_wrap(&cfg, PWM_TOP);
    pwm_config_set_phase_correct(&cfg, false);
    pwm_init(s, &cfg, false);
  }

  pwm_set_output_polarity(PWM_SLICE_A_LO, true, false);   // GPIO2 inverted
  pwm_set_output_polarity(PWM_SLICE_B_LO, true, false);   // GPIO4 inverted

  // The brief writes "phase of -k*dt" with counters T-dt and T-2dt, but that
  // gives a delayed low-side slice and creates shoot-through at the period
  // boundary.  Advancing the low-side slices by +dt and +2dt with the same
  // compare values produces the stated dead-time gap on both edges.
  pwm_set_counter(PWM_SLICE_A_HI, 0);
  pwm_set_counter(PWM_SLICE_A_LO, (uint16_t)PWM_DT_TICKS);
  pwm_set_counter(PWM_SLICE_B_LO, (uint16_t)(2 * PWM_DT_TICKS));

  writeBridgeDuty(0.0f);

  pwm_set_mask_enabled((1u << PWM_SLICE_A_HI) |
                       (1u << PWM_SLICE_A_LO) |
                       (1u << PWM_SLICE_B_LO));

  // Map the gate pins to the PWM peripheral now that every slice is already
  // running with the correct levels: high sides high, low sides low.  Mapping
  // before enabling risks the PWM peripheral's disabled-state level driving a
  // gate on (in particular the inverted low-side channels).
  gpio_set_function(GATE_A_HI_PIN, GPIO_FUNC_PWM);
  gpio_set_function(GATE_A_LO_PIN, GPIO_FUNC_PWM);
  gpio_set_function(GATE_B_HI_PIN, GPIO_FUNC_PWM);
  gpio_set_function(GATE_B_LO_PIN, GPIO_FUNC_PWM);

  // Carrier-synchronous modulation: the slice-0 wrap ISR advances the sine
  // and writes all four compares once per 20 kHz period.
  sinePhase = 0;
  modAmp = 0.0f;
  static bool pwmIrqReady = false;
  if (!pwmIrqReady) {
    irq_set_exclusive_handler(PWM_IRQ_WRAP, onPwmWrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwmIrqReady = true;
  }
  pwm_set_irq_enabled(PWM_SLICE_A_HI, true);
  bridgeModulating = true;
}

static void startBridge() {
  if (pState == POWER_FAULT) return;
  if (!pArmed) return;
  if (!sensorsOk) return;
  if (!busValidForStart()) return;
  resetMeasurementWindow();
  busBadTicks = 0;
  // NOTE: inputRailWasValid is deliberately NOT cleared here.  This function
  // runs only from the WAIT_BUS branch, which proves the rail live on the
  // same tick (and every tick while waiting), so there is no stale state to
  // discard.  Clearing it would force one un-hysteretic sample on the first
  // powered tick, where a battery sitting exactly on the 11.0 V edge plus a
  // millivolt of noise coin-flips an instant "input rail" fault.
  // (stopBridge() still resets it; the next proof happens in IDLE/WAIT_BUS,
  // where a false reading only inhibits, never faults.)
  rmsInvalidSinceMs = 0;
  enablePwmSlices();
  rampStartMs = millis();
  mRamp = 0.0f;
  mTarget = 0.0f;
  pState = POWER_STARTING;
}

static void runControl(uint32_t nowMs) {
  if (nowMs - lastCtrlMs < CTRL_INTERVAL_MS) return;
  lastCtrlMs = nowMs;

  // Refresh the droop-compensated outlet estimate once per control tick.
  // Everything below (the overvoltage trip and the amplitude regulator)
  // works on this estimate; it is the only outlet number the board has.
  updateOutletEstimate();

  // Warnings (do not trip).  Latched until the condition clears, and reset
  // by stopBridge() so a fault clear re-warns if the condition is still
  // present -- otherwise a persistent over-current would print once and
  // never again.
  if (iIn >= IIN_WARN && !warnedCurrent) {
    warnedCurrent = true;
    if (Serial) Serial.println("WARN: input current");
  } else if (iIn < IIN_WARN - 1.0f) {
    warnedCurrent = false;
  }
  bool ctrlHot = tempCtrlValid && (tempCtrl >= TEMP_WARN_C);
  bool invHot  = tempInvValid  && (tempInv  >= TEMP_WARN_C);
  if ((ctrlHot || invHot) && !warnedTemp) {
    warnedTemp = true;
    if (Serial) Serial.println("WARN: temperature");
  } else if (!ctrlHot && !invHot &&
             (!tempCtrlValid || tempCtrl < TEMP_WARN_C - 5.0f) &&
             (!tempInvValid || tempInv < TEMP_WARN_C - 5.0f)) {
    warnedTemp = false;
  }

  // Protections are only evaluated while the bridge is armed.  A voltage
  // reading alone is not proof the hardware is safe to switch.
  if (pArmed) {
    // Sensor liveness first.  A probe that succeeded once is not proof the
    // chip is still answering: adsFail() catches a NACK, but a chip that
    // stops converting without NACKing, or a bus that goes quiet, would leave
    // the last good values in place and the protections comparing stale data.
    // Checking this before the rail/bus comparisons attributes a dead bus to
    // the sensor instead of to whichever voltage happened to be checked first.
    uint32_t nowUs = micros();
    bool adsAlive = adsPresent &&
                    adsFresh(0, nowUs) && adsFresh(1, nowUs) &&
                    adsFresh(2, nowUs) && adsFresh(3, nowUs) &&
                    adsFresh(4, nowUs);
    // A lost sensor is a fault while the bridge is driving; while it is
    // armed but idle, sensorsOk going false simply prevents a start.
    if ((pState == POWER_RUNNING || pState == POWER_STARTING) && !adsAlive) {
      setFault("sensor stale");
      return;
    }
    // Over-current, debounced like the other fast trips: a single noisy
    // INA240 sample must not stop the inverter, and 20 ms is far shorter
    // than any thermal or magnetic consequence at 20 A.
    if (iIn >= IIN_TRIP) {
      if (++iinOverCount >= IIN_TRIP_TICKS) { setFault("over-current"); return; }
    } else {
      iinOverCount = 0;
    }
    // Input rail, debounced like the other trips: one bad sample with the
    // bridge armed must not stop the inverter, and 30 ms is far shorter
    // than any consequence of a genuinely lost input.
    if (!inputRailValid()) {
      if (++railBadTicks >= RAIL_TRIP_TICKS) { setFault("input rail"); return; }
    } else {
      railBadTicks = 0;
    }

    if (pState == POWER_RUNNING || pState == POWER_STARTING) {
      if (!busValidForRun()) { setFault("bus voltage"); return; }

      // Temperature validity and over-temperature.  A reading outside the
      // TMP235's -40..150 C range is an open or shorted sensor, not a
      // temperature: a short reads cold and would hide a real over-temp, an
      // open reads hot and would invent one.  Both conditions are debounced
      // over TEMP_TRIP_TICKS (30 ms), which is longer than a converter
      // glitch and far shorter than any real thermal event.
      if (!tempCtrlValid || !tempInvValid) {
        if (++tempInvalidCount >= TEMP_TRIP_TICKS) {
          setFault("temperature sensor");
          return;
        }
      } else {
        tempInvalidCount = 0;
        if (tempCtrl >= TEMP_TRIP_C) {
          if (++tempCtrlOverCount >= TEMP_TRIP_TICKS) {
            setFault("over-temp CTRL");
            return;
          }
        } else {
          tempCtrlOverCount = 0;
        }
        if (tempInv >= TEMP_TRIP_C) {
          if (++tempInvOverCount >= TEMP_TRIP_TICKS) {
            setFault("over-temp INV");
            return;
          }
        } else {
          tempInvOverCount = 0;
        }
      }

      // Measurement supervision.  The fit is allowed to be invalid for the
      // few milliseconds it takes to fill at the start of a run, but a run
      // that never regains a drive measurement has lost its DC, overvoltage
      // and bridge-plausibility checks at once.  Treat that as a fault after
      // RMS_INVALID_TRIP_MS rather than regulating on a stale estimate.
      if (!rmsValid) {
        if (rmsInvalidSinceMs == 0) {
          rmsInvalidSinceMs = nowMs ? nowMs : 1;
        } else if (nowMs - rmsInvalidSinceMs >= RMS_INVALID_TRIP_MS) {
          setFault("no drive measurement");
          return;
        }
      } else {
        rmsInvalidSinceMs = 0;
      }

      // Plausibility: the bridge midpoint differential must track the
      // commanded modulation.  This monitors the BRIDGE, not the outlet: an
      // open transformer secondary is invisible to these dividers.
      if (rmsValid) {
        // Overvoltage on the ESTIMATED outlet.  The estimate is
        // droop-compensated, so this is the receptacle voltage the
        // estimate believes is present; the hard ceiling is
        // OUTLET_V_MAX (125 V) and this trip is the backstop above it.
        // Two ticks (20 ms) is long enough to reject a single fit outlier
        // and short enough to keep the drive inside the receptacle rating.
        if (outletEstimate() > OUTPUT_TARGET_RMS * OUTPUT_OV_FACTOR) {
          if (++outOvCount >= OUTPUT_OV_TICKS) {
            setFault("output overvoltage");
            return;
          }
        } else {
          outOvCount = 0;
        }
        float busForCheck = (vBus > 1.0f) ? vBus : VBUS_NOMINAL;
        // Expected bridge fundamental on the outlet scale, using the
        // NO-LOAD ratio — the same ratio the droop model uses, so the
        // two never disagree.  (The ratio cancels in the comparison
        // below; it is spelled out so the check is readable.)
        float expected = modulation * busForCheck * TRANSFORMER_NOLOAD_RATIO / SQRT2;
        if (expected > 15.0f &&
            inferredOutletRms < OUT_MISSING_FRACTION * expected) {
          if (++outMissingCount >= 5) {
            setFault("bridge output missing");
            return;
          }
        } else {
          outMissingCount = 0;
        }
        // The DC-offset trip is NOT here.  It is evaluated in
        // updateRmsAndDc(), on every RMS sample, because it has to react
        // within the same millisecond the offset appears -- see the
        // DC_TRIP_V comment.
      }
    } else {
      // Armed but not driving: no thermal or measurement supervision is
      // needed, and the debounce counters must not carry into a later run.
      tempCtrlOverCount = 0;
      tempInvOverCount = 0;
      tempInvalidCount = 0;
      iinOverCount = 0;
      railBadTicks = 0;
    }
  }

  // Soft-start ramp: raise the modulation amplitude over RAMP_TIME_MS, then
  // hand over to the regulator.
  if (pState == POWER_STARTING) {
    uint32_t elapsed = nowMs - rampStartMs;
    if (elapsed >= RAMP_TIME_MS) {
      pState = POWER_RUNNING;
      mTarget = mRamp;
    } else {
      mRamp = M_START * ((float)elapsed / (float)RAMP_TIME_MS);
    }
  }

  // Amplitude regulation once running.
  //
  // The regulator drives the ESTIMATED OUTLET to OUTPUT_TARGET_RMS.  The
  // estimate is a droop-compensated feed forward (the drive and the load
  // both feed it) closed by an integral action on the estimate itself, so
  // an error in B_DROOP shows up as a small steady-state error at load
  // rather than as instability.
  //
  // The estimate is refreshed once per control tick (CTRL_INTERVAL_MS =
  // 10 ms, so 100 Hz) and the 120 Hz load pulsation is filtered out by
  // that loop.  The loop is therefore slower than the estimate can track,
  // which is what keeps the feed forward from exciting the loop.
  if (pState == POWER_RUNNING && rmsValid) {
    float pOut = outputPowerEstimate();
    float y = B_DROOP * pOut;
    if (y < 0.0f) y = 0.0f;

    // Hard ceiling: the drive may not push the estimated outlet past
    // OUTLET_V_MAX (the 125 V receptacle rating).  At zero load this is
    // the plain no-load bound; at load the droop allows more drive,
    // because the estimate falls with the load.
    float driveLimit = outletDriveLimit(y);

    // The modulation that WOULD hold OUTPUT_TARGET_RMS at the estimated
    // load: holding the target needs
    //     x_req = OUTPUT_TARGET_RMS + y / OUTPUT_TARGET_RMS
    // of drive, and the modulation that makes that drive is
    //     m = x_req * sqrt(2) / (A_DROOP * Vbus).
    // It is kept for the serial report so the bench can see the margin
    // to the clamp.
    float busForCalc = (vBus > 1.0f) ? vBus : VBUS_NOMINAL;
    float mReq = (OUTPUT_TARGET_RMS + y / OUTPUT_TARGET_RMS) * SQRT2 /
                 (A_DROOP * busForCalc);
    mReqNow = mReq;
    driveLimitNow = driveLimit;

    // ---- OUTPUT OUT OF REACH --------------------------------------
    // The modulation ceiling is tight on this design: 120.0 V at 100 W
    // needs m = 0.941 at the bottom of the bus band against a clamp of
    // 0.9597, so a transformer that regulates worse than the 8 %
    // catalogue figure, or a bus that has sagged towards the bottom of
    // its run window with a heavy load attached, simply cannot reach
    // the target.  When the regulator is pinned at the PWM CLAMP and
    // the estimated outlet is still below the target, the target is out
    // of reach: latch a fault instead of silently saturating.
    //
    // It is evaluated before the load limit below, and neither of the
    // checks above can mask it: an overvoltage requires the estimate to
    // be above 126 V, the opposite of below-target, and the bridge-missing
    // check requires the measured drive to be far BELOW the commanded
    // drive, where this check requires it to track the clamp.  The
    // debounce (200 ms) exists because the integrator needs about 100 ms
    // to walk into the clamp after a load step.
    //
    // Only the PWM clamp counts.  The drive ceiling also carries the
    // OUTLET_V_MAX receptacle limit, which binds at low load; sitting
    // at that ceiling is correct behaviour, not a failure.
    bool clampBound  = (driveLimit >= PWM_M_MAX - 1e-4f);
    bool atCeiling   = (mTarget >= driveLimit - 1e-4f);
    bool belowTarget = (outletEstimate() < OUTPUT_TARGET_RMS - OUT_OF_REACH_BAND_V);
    regSaturated = atCeiling;
    atPwmClamp = clampBound;
    if (clampBound && atCeiling && belowTarget) {
      if (++outOfReachCount >= OUT_OF_REACH_TICKS) {
        setFault("output out of reach");
        return;
      }
    } else {
      outOfReachCount = 0;
    }

    // ---- LOAD TOO HEAVY -------------------------------------------
    // The transformer, not the modulator, is the limit: the VPT24-6670
    // is 160 VA, and above LOAD_LIMIT_W the outlet is asking for more
    // than the winding can pass continuously.  The output power is an
    // ESTIMATE (battery current times an efficiency constant), so the
    // limit is debounced (500 ms) and it is only evaluated once the
    // bridge is running — never during the soft-start ramp, where the
    // inrush would trip it.
    if (pOut > LOAD_LIMIT_W) {
      if (++loadHeavyCount >= LOAD_HEAVY_TICKS) {
        setFault("load too heavy");
        return;
      }
    } else {
      loadHeavyCount = 0;
    }

    // Integral action on the estimated outlet.  mTarget is re-clamped to
    // the drive limit on every tick, so the integrator cannot wind up
    // against the ceiling.
    float error = OUTPUT_TARGET_RMS - outletEstimate();
    float step = error * AMP_KI;
    if (step > AMP_SLEW_MAX) step = AMP_SLEW_MAX;
    if (step < -AMP_SLEW_MAX) step = -AMP_SLEW_MAX;
    mTarget += step;
    mTarget = clampf(mTarget, 0.0f, driveLimit);
    mRamp = mTarget;
  } else {
    // The regulator is not running (still ramping, or the RMS window is
    // not yet full): there is no margin to report.
    mReqNow = 0.0f;
    driveLimitNow = 0.0f;
    regSaturated = false;
    atPwmClamp = false;
  }

  modulation = mRamp;
  if (modulation > PWM_M_MAX) modulation = PWM_M_MAX;
  if (modulation < 0.0f) modulation = 0.0f;
  modAmp = modulation;   // read by the 20 kHz wrap ISR
}

void powerBegin() {
  // Park gates low before any other peripheral is touched.
  pinMode(GATE_A_HI_PIN, OUTPUT);
  pinMode(GATE_A_LO_PIN, OUTPUT);
  pinMode(GATE_B_HI_PIN, OUTPUT);
  pinMode(GATE_B_LO_PIN, OUTPUT);
  digitalWrite(GATE_A_HI_PIN, LOW);
  digitalWrite(GATE_A_LO_PIN, LOW);
  digitalWrite(GATE_B_HI_PIN, LOW);
  digitalWrite(GATE_B_LO_PIN, LOW);

  // I2C for ADS7128: 400 kHz is the operating bus.  A 100 kHz fallback is
  // kept for DETECTION only.  The weighted sweep gives each midpoint
  // channel a conversion about every 2 ms, and at 100 kHz one 3-byte
  // transfer takes over 300 us, so the sweep would starve and the DC/RMS
  // fit would not be a one-mains-period estimate.  Arming therefore
  // requires a confirmed 400 kHz bus (adsClockOk), and powerService keeps
  // retrying 400 kHz while the bridge is disarmed.
  //
  // The default Wire timeout is the inherited Stream 1000 ms; cap it at
  // 2 ms so a stuck bus cannot freeze the bridge long enough to saturate
  // the transformer.
  Wire.setSDA(8);
  Wire.setSCL(9);
  Wire.setTimeout(2);
  Wire.begin();
  Wire.setClock(400000);
  adsPresent = adsProbe(true);
  if (!adsPresent) {
    // Find out whether the chip is present at all, so the warning is true.
    Wire.setClock(100000);
    adsPresent = adsProbe(false);
    if (adsPresent) {
      adsSlowWarned = true;
      if (Serial) Serial.println("WARN: ADS7128 only at 100 kHz; arming disabled");
    }
  }
  if (!adsPresent) {
    if (Serial) Serial.println("WARN: ADS7128 not found at boot");
  }

  // RP2350 ADC for TMP235 temperature sensors.
  adc_init();
  adc_gpio_init(TEMP_CTRL);
  adc_gpio_init(TEMP_INV);

  // Hardware watchdog.  Safe because the UCC27735 inputs have internal
  // pull-downs that bring the gates low while the MCU is in reset.  The
  // boot sweep is well under 200 ms, but 500 ms keeps
  // margin without weakening runtime hang detection.
  watchdog_enable(500, true);
}

void powerService(uint32_t nowMs) {
  watchdog_update();

  // Throttled ADS7128 probe retry, while disarmed and not faulted.  Try the
  // 400 kHz operating bus first every time; the 100 kHz probe is only there
  // to keep detection alive when the high-speed bus fails, and it never
  // clears the arming refusal.
  if ((!adsPresent || !adsClockOk) && !pArmed && !pFault &&
      (nowMs - lastProbeMs >= 500)) {
    lastProbeMs = nowMs;
    Wire.setClock(400000);
    if (adsProbe(true)) {
      adsPresent = true;
      adsSlowWarned = false;
    } else {
      Wire.setClock(100000);
      adsPresent = adsProbe(false);
      if (adsPresent && !adsSlowWarned) {
        adsSlowWarned = true;
        if (Serial) Serial.println("WARN: ADS7128 only at 100 kHz; arming disabled");
      }
    }
  }

  adsService();
  readTemperatures();
  updateRmsAndDc();
  runControl(nowMs);

  // sensorsOk = probe succeeded at 400 kHz AND a full weighted sweep has
  // completed AND every channel is fresh AND both temperature readings are
  // plausible.  The clock and freshness conditions are deliberate: the
  // protections and the regulator are only as good as the data behind them,
  // so a chip that is present but not being read on time is not "sensors
  // OK" and must not be armed.
  uint32_t sensorNowUs = micros();
  sensorsOk = adsPresent && adsClockOk && adsSwept &&
              adsFresh(0, sensorNowUs) && adsFresh(1, sensorNowUs) &&
              adsFresh(2, sensorNowUs) && adsFresh(3, sensorNowUs) &&
              adsFresh(4, sensorNowUs) &&
              tempCtrlValid && tempInvValid;

  // Disarmed → never leave IDLE.
  if (!pArmed) {
    if (pState != POWER_FAULT) pState = POWER_IDLE;
    // Only park when the bridge is actually modulating.  stopBridge() is
    // already called by setFault(), powerSetArmed(false) and powerClearFault();
    // re-running it every loop while idle just churns the gate pins.
    if (pState == POWER_IDLE && bridgeModulating) stopBridge();
  }

  // Power-mode detection: the input rail measurement decides which
  // personality the board wears.  Entering inverter mode needs a fresh AIN0
  // sample; leaving it needs AIN0 to have been read within RAIL_MODE_STALE_US,
  // so a loop stall cannot flip the UI.  At boot (no sweep yet) the board is
  // in 5V mode, which is also the safe side to fail toward.  Either transition
  // parks the bridge and returns to idle: the home menu has a different
  // item count per mode, so no cursor or mode survives the flip.  A flip
  // while asleep changes the mode but does not wake the board.
  bool railSeen = adsChTime[0] != 0 &&
                  (uint32_t)(sensorNowUs - adsChTime[0]) <= RAIL_MODE_STALE_US;
  if (!inverterPower && adsFresh(0, sensorNowUs) && vIn >= INVERTER_ENTER_V) {
    inverterPower = true;
    if (Serial) Serial.println("MODE: INVERTER");
    powerSetArmed(false);
    if (uiState != UI_SLEEP) returnToIdle();   // asleep: only the button wakes it
  } else if (inverterPower && (!railSeen || vIn < INVERTER_EXIT_V)) {
    inverterPower = false;
    if (Serial) Serial.println("MODE: 5V");
    powerSetArmed(false);
    if (uiState != UI_SLEEP) returnToIdle();   // asleep: only the button wakes it
  }

  // State transitions (armed only).
  if (pArmed && !pFault) {
    if (pState == POWER_IDLE) {
      if (sensorsOk && inputRailValid()) {
        pState = POWER_WAIT_BUS;
      }
    } else if (pState == POWER_WAIT_BUS) {
      if (!inputRailValid()) {
        pState = POWER_IDLE;
      } else if (busValidForStart()) {
        startBridge();
      }
    } else if (pState == POWER_STARTING || pState == POWER_RUNNING) {
      // Handled in runControl(); fault path is a latch.
    }
  }

  // Serial state-change and one-second reporting.
  if (Serial) {
    if (pState != lastReportedState) {
      lastReportedState = pState;
      Serial.print("STATE: ");
      Serial.println(powerStateName());
    }
    if (nowMs - lastReportMs >= 1000) {
      lastReportMs = nowMs;
      powerSerialReport();
    }
  }
}

bool powerBridgeRunning() {
  return pState == POWER_STARTING || pState == POWER_RUNNING;
}

uint8_t powerState() {
  return (uint8_t)pState;
}

const char *powerStateName() {
  switch (pState) {
    case POWER_IDLE:      return "IDLE";
    case POWER_WAIT_BUS:  return "WAIT_BUS";
    case POWER_STARTING:  return "STARTING";
    case POWER_RUNNING:   return "RUNNING";
    case POWER_FAULT:     return "FAULT";
  }
  return "UNKNOWN";
}

float powerInputVolts()  { return vIn; }
float powerBusVolts()    { return vBus; }
// The ESTIMATED outlet voltage (droop-compensated), not a measurement.
float powerOutputVolts() { return outletEstimate(); }
// The BATTERY current, measured by the INA240 across RCS1 -- not an
// output current.  The board has no output current measurement.
float powerInputAmps()   { return iIn; }

float powerTempC(uint8_t index) {
  if (index == 0) return tempCtrl;
  if (index == 1) return tempInv;
  return 0.0f;
}

bool powerFaulted() {
  return pFault;
}

void powerClearFault() {
  if (!pFault) return;
  // A clear attempt never leaves the bridge armed, even if it is refused:
  // setFault() already disarmed, and enforcing it again costs nothing.
  pArmed = false;
  // Over-current and over-temperature require a physical reset for safety.
  if (strncmp(pFaultReason, "over-", 5) == 0) {
    if (Serial) Serial.println("CLEAR ignored: over-fault latched");
    return;
  }
  pFault = false;
  pState = POWER_IDLE;
  memset(pFaultReason, 0, sizeof(pFaultReason));
  // Force a fresh 400 kHz probe and a full sweep; a bus glitch may have left
  // the ADS7128 mid-sequence even though the MCU kept running.  The clock
  // flag is cleared too, so the retry path has to prove the high-speed bus
  // before the next arm.
  adsPresent = false;
  adsClockOk = false;
  adsSwept = false;
  adsPhase = ADS_IDLE;
  adsConvStartUs = 0;
  for (uint8_t i = 0; i < ADS_CH_COUNT; i++) adsChTime[i] = 0;
  lastProbeMs = 0;
  stopBridge();
  if (Serial) Serial.println("Fault cleared (disarmed)");
}

bool powerArmed() { return pArmed; }

bool powerSensorsOk() { return sensorsOk; }

bool powerSetArmed(bool on) {
  if (!on) {
    if (!pArmed && pState != POWER_FAULT && !bridgeModulating) {
      return true;              // already disarmed and parked
    }
    pArmed = false;
    stopBridge();
    if (pState != POWER_FAULT) pState = POWER_IDLE;
    if (Serial) Serial.println("DISARMED");
    return true;
  }
  // Arming is idempotent.  Resetting the state machine on a repeated ARM
  // used to drop a RUNNING bridge back to IDLE, where powerService() parks
  // the gates: a second "arm" request stopped the inverter.
  if (pArmed) return true;
  if (!inverterPower) {
    if (Serial) Serial.println("ARM refused: no battery (5V mode)");
    return false;
  }
  if (pFault) {
    if (Serial) Serial.println("ARM refused: fault latched");
    return false;
  }
  if (!sensorsOk) {
    // Covers the 100 kHz fallback, a stale channel and an implausible
    // temperature as well as a missing chip.
    if (!adsPresent) {
      if (Serial) Serial.println("ARM refused: no sensors");
    } else if (!adsClockOk) {
      if (Serial) Serial.println("ARM refused: I2C not at 400 kHz");
    } else {
      if (Serial) Serial.println("ARM refused: sensors not ready");
    }
    return false;
  }
  if (!inputRailValid()) {
    // Arming with the rail outside 11-14 V would only latch an input-rail
    // fault 30 ms later, after nothing was ever driven.  Refuse here instead,
    // where nothing is latched.
    if (Serial) Serial.println("ARM refused: input rail out of 11-14 V");
    return false;
  }
  pArmed = true;
  pState = POWER_IDLE;
  if (Serial) Serial.println("ARMED");
  return true;
}

void powerSerialReport() {
  if (!Serial) return;
  Serial.print(powerStateName());
  Serial.print(" ARMED="); Serial.print(powerArmed() ? "Y" : "N");
  Serial.print(" SENS=");  Serial.print(powerSensorsOk() ? "Y" : "N");
  Serial.print(" MODE=");  Serial.print(inverterPower ? "INV" : "5V");
  Serial.print(" I2C=");   Serial.print(adsClockOk ? 400 : (adsPresent ? 100 : 0));
  Serial.print(" VIN=");   Serial.print(vIn);
  Serial.print(" VBUS=");  Serial.print(vBus);
  Serial.print(" VOUT=");  Serial.print(outletEstimate());
  Serial.print(" VPRI=");  Serial.print(drivePrimaryRms());
  Serial.print(" VDC=");   Serial.print(primaryDc);
  Serial.print(" RMSV=");  Serial.print(rmsValid ? 1 : 0);
  Serial.print(" POUT=");  Serial.print(outputPowerEstimate());
  Serial.print(" ISEC=");  Serial.print(secCurrentEst);
  Serial.print(" UNSUP="); Serial.print(loadUnsupported ? 1 : 0);
  Serial.print(" IIN=");   Serial.print(iIn);
  Serial.print(" Tctrl="); Serial.print(tempCtrl);
  Serial.print(" Tinv=");  Serial.print(tempInv);
  // The modulation margin.  m is what the bridge is actually doing,
  // MMAX is the hard clamp, MREQ is what the target needs at the present
  // load and bus, DLIM is the ceiling the regulator is held to (the
  // 125 V receptacle limit, or the clamp), SAT says the regulator is
  // pinned at that ceiling and CLAMP says the ceiling is the clamp.
  Serial.print(" m=");     Serial.print(modulation, 3);
  Serial.print(" MMAX=");  Serial.print(PWM_M_MAX, 4);
  Serial.print(" MREQ=");  Serial.print(mReqNow, 3);
  Serial.print(" DLIM=");  Serial.print(driveLimitNow, 3);
  Serial.print(" SAT=");   Serial.print(regSaturated ? 1 : 0);
  Serial.print(" CLAMP="); Serial.print(atPwmClamp ? 1 : 0);
  Serial.println();
}

/* ================================================================
 * SETUP
 * ================================================================ */
void setup() {
  Serial.begin(115200);

  // Power stage first: gates low before anything else can wiggle them.
  powerBegin();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setDither(0);
  applyBrightness();
  FastLED.clear(true);

  entropy.begin();
  button.begin();

  // Known-good startup sweep scaled to five LEDs.
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(i * 51, 230, 210);
    FastLED.show();
    delay(18);
    watchdog_update();
  }
  delay(70);
  watchdog_update();
  clearLeds();
  FastLED.show();

  currentMainMode = 0;
  mainCursor = 0;
  uiState = UI_MODE;
  modeStatus.enter();
}

/* ================================================================
 * LOOP
 * ================================================================ */
void loop() {
  static uint32_t lastFrame = 0;
  uint32_t now = millis();

  // Power-stage service runs every iteration and is strictly non-blocking.
  powerService(now);

  entropy.update();

  // Sleep blanks the LEDs once, in enterShutdown().  The loop itself must not
  // be throttled here: the ADS7128 state machine advances one step per loop
  // pass, so a delay() here stretches each sweep to ~225 ms and the battery
  // reading goes stale.
  if (uiState == UI_SLEEP) {
    updateShutdown();
    return;
  }


  if (inAnyMenu() && now - menuLastActivity > MENU_TIMEOUT_MS) {
    returnToIdle();
  }

  // Treat the Inverter portal as a non-menu for gesture synthesis so its
  // double-tap (fault clear) is produced; other menus keep immediate taps.
  Gesture g = button.poll(inAnyMenu() && uiState != UI_INVERTER);
  if (g != G_NONE) handleUI(g);

  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    renderUI();
    FastLED.show();
  }
}

/* ================================================================
 * INVERTER PORTAL RENDER  (defined here so the PowerState enum is in scope)
 * ================================================================ */
void renderInverter() {
  clearLeds();

  if (powerFaulted()) {
    uint8_t p = beatsin8(6, 40, 220);
    fill_solid(leds, NUM_LEDS, CRGB(p, 0, 0));
    return;
  }

  if (!powerSensorsOk()) {
    leds[0] = CRGB(180, 90, 0);
    return;
  }

  if (!powerArmed()) {
    leds[0] = CRGB(0, 25, 70);
    return;
  }

  uint8_t st = powerState();
  if (st == POWER_WAIT_BUS) {
    uint8_t p = beatsin8(4, 30, 180);
    leds[0] = CRGB(p, p / 2, 0);
    return;
  }

  if (st == POWER_STARTING) {
    float v = powerBusVolts();
    float frac = v / VBUS_NOMINAL;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    uint8_t lit = (uint8_t)(frac * NUM_LEDS);
    if (lit > NUM_LEDS) lit = NUM_LEDS;
    for (uint8_t i = 0; i < lit; i++) leds[i] = COLOR_CACTUS;
    if (lit < NUM_LEDS) leds[lit] = CRGB(0, 40, 10);
    return;
  }

  if (st == POWER_RUNNING) {
    // Bar shows the ESTIMATED outlet against the 120 V target.
    float frac = powerOutputVolts() / OUTPUT_TARGET_RMS;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    uint8_t lit = (uint8_t)(frac * NUM_LEDS);
    if (lit == 0) lit = 1;
    if (lit > NUM_LEDS) lit = NUM_LEDS;
    for (uint8_t i = 0; i < lit; i++) {
      uint8_t b = beatsin8(3, 120, 255);
      leds[i] = CRGB(0, b, 0);
    }
    return;
  }

  // POWER_IDLE (armed, waiting for a valid input rail): dim amber.
  leds[0] = CRGB(120, 60, 0);
}
