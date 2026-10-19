#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include "pico/bootrom.h"
#include "pico/rand.h"
#include "hardware/pwm.h"
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
void renderInverter();

/*
 * ================================================================
 * LAMOKA1/2 MAIN BOARD — PORTAL INTERFACE ENGINE V1.9
 * ================================================================
 *
 * First PIE revision that runs on the main Lamoka1/2 board and drives
 * the H-bridge power stage.  Derived from V1.8 (DEV-Brain UI firmware);
 * the UI gestures, effects and style are preserved and scaled to the
 * main board's five WS2812B LEDs.
 *
 * HOME
 * ----
 * 0 = BLUE STATUS BALL
 * 1 = FIREPLACE
 * 2 = LIGHTSABER
 * 3 = RAINBOW
 * 4 = GAMES
 * 5 = SETTINGS
 *
 * HOME COLORS
 * -----------
 * 0-3 = exact cactus green
 * 4   = games orange
 * 5   = settings blue
 *
 * CURSOR
 * ------
 * Every selected item is hot pink (breathing).
 * Cursors remember the last selected item.
 *
 * DEFAULT IDLE
 * ------------
 * Blue bouncing status ball.
 *
 * FIREPLACE / LIGHTSABER / RAINBOW / FIREBALL
 * -------------------------------------------
 * Same behaviour as V1.8, scaled to five LEDs.
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
const CRGB COLOR_GAMES    = CRGB(255, 90, 0);
const CRGB COLOR_SETTINGS = CRGB(0, 50, 100);

const CRGB FIRE_GREEN  = CRGB(0, 180, 40);
const CRGB FIRE_RED    = CRGB(150, 0, 0);
const CRGB FIRE_ORANGE = CRGB(255, 45, 0);
const CRGB FIRE_HOT    = CRGB(255, 130, 10);
const CRGB FIRE_WHITE  = CRGB(255, 255, 255);

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
  UI_MENU_GAMES,
  UI_MENU_SETTINGS,
  UI_BRIGHTNESS,
  UI_SPEED,
  UI_INFO,
  UI_INVERTER,
  UI_IN_GAME,
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
uint8_t gamesCursor          = 0;   // remembers last Games selection
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

void renderCursor(uint8_t index, uint8_t minimumBrightness) {
  if (index >= NUM_LEDS) return;
  CRGB c = COLOR_HOT_PINK;
  c.nscale8(beatsin8(7, minimumBrightness, 255));
  leds[index] = c;
}

/* ================================================================
 * ENTROPY SERVICE  (RP2350 hardware TRNG)
 * ================================================================ */
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
 * FIREPLACE — V1.8 REALISTIC UPGRADE, scaled to five LEDs
 * ================================================================ */
class ModeFire : public Mode {
  uint8_t heat[NUM_LEDS];
  int16_t source = 2;
  int16_t target = 2;
  int8_t  sourceVel = 0;
  uint32_t lastMove = 0;
  uint8_t flare = 0;

  void chooseTarget() {
    target = (int16_t)(1 + entropy.range(NUM_LEDS - 2));
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
    if (source > NUM_LEDS - 2)  { source = NUM_LEDS - 2;  sourceVel = 0; }
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

    if (entropy.next8() > 245) {
      uint8_t spark = (uint8_t)(1 + entropy.range(3));
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
 * LIGHTSABER — never same hue twice in a row
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
 * RAINBOW
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
 * FIREBALL GAME
 * ================================================================ */
class AppFireball {
  static const int TARGET_LED  = 0;
  static const int START_SPEED = 115;
  static const int SPEEDUP     = 10;
  static const int MIN_SPEED   = 30;
  static const int MAX_SCORE   = 31;   // 5-bit binary score display

  enum GameState : uint8_t {
    GAME_RUNNING,
    GAME_BLAST,
    GAME_SCORE,
    GAME_MISSED,
    GAME_OVER
  };

  GameState state        = GAME_RUNNING;
  uint8_t   score        = 0;
  uint8_t   lives        = 3;
  int       firePosition = NUM_LEDS - 1;
  int       fireSpeed    = START_SPEED;
  uint32_t  lastMove     = 0;
  uint32_t  stateStart   = 0;

  uint8_t displayedScore() const {
    return score > MAX_SCORE ? MAX_SCORE : score;
  }

  void drawGame() {
    clearLeds();
    leds[TARGET_LED] = FIRE_GREEN;

    if (firePosition >= 0 && firePosition < NUM_LEDS) {
      leds[firePosition] = FIRE_WHITE;
    }
    if (firePosition + 1 < NUM_LEDS) leds[firePosition + 1] = FIRE_HOT;
    if (firePosition + 2 < NUM_LEDS) leds[firePosition + 2] = FIRE_ORANGE;
    if (firePosition + 3 < NUM_LEDS) leds[firePosition + 3] = FIRE_RED;
  }

  void beginRound() {
    firePosition = NUM_LEDS - 1;
    lastMove = millis();
    state = GAME_RUNNING;
  }

  void startBlast() {
    stateStart = millis();
    state = GAME_BLAST;
  }

  void catchFireball() {
    if (score < 255) score++;
    fireSpeed -= SPEEDUP;
    if (fireSpeed < MIN_SPEED) fireSpeed = MIN_SPEED;
    startBlast();
  }

  void missFireball() {
    if (lives > 0) lives--;
    stateStart = millis();
    state = (lives == 0) ? GAME_OVER : GAME_MISSED;
  }

  void renderBlast() {
    clearLeds();
    uint32_t elapsed = millis() - stateStart;
    uint8_t frame = (uint8_t)(elapsed / 45UL);
    if (frame > 7) frame = 7;

    for (int i = 0; i < NUM_LEDS; i++) {
      int distance = abs(i - TARGET_LED);
      if (distance <= frame) {
        int level = 255 - distance * 50 - frame * 18;
        if (level < 10) level = 10;
        leds[i] = CRGB((uint8_t)level, (uint8_t)(level / 2), (uint8_t)(level / 12));
      }
    }
  }

  void renderScore() {
    clearLeds();
    uint8_t shown = displayedScore();
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      if (shown & (1U << i)) leds[i] = COLOR_CACTUS;
    }
  }

public:
  void enter() {
    score = 0;
    lives = 3;
    fireSpeed = START_SPEED;
    firePosition = NUM_LEDS - 1;
    lastMove = millis();
    state = GAME_RUNNING;
    stateStart = millis();
  }

  void update(Gesture g) {
    if (state == GAME_OVER) {
      if (g == G_TAP) enter();
      return;
    }
    if (state == GAME_BLAST || state == GAME_SCORE || state == GAME_MISSED) return;

    if (state == GAME_RUNNING && g == G_TAP) {
      if (firePosition == TARGET_LED) catchFireball();
    }
  }

  void render() {
    if (state == GAME_RUNNING) {
      uint32_t now = millis();
      if (now - lastMove >= (uint32_t)fireSpeed) {
        lastMove = now;
        firePosition--;
        if (firePosition < TARGET_LED) missFireball();
      }
      if (state == GAME_RUNNING) drawGame();
      return;
    }

    if (state == GAME_BLAST) {
      uint32_t elapsed = millis() - stateStart;
      renderBlast();
      if (elapsed >= 360) {
        state = GAME_SCORE;
        stateStart = millis();
      }
      return;
    }

    if (state == GAME_SCORE) {
      renderScore();
      if (millis() - stateStart >= 750) beginRound();
      return;
    }

    if (state == GAME_MISSED) {
      clearLeds();
      uint32_t elapsed = millis() - stateStart;
      if (elapsed < 70) fill_solid(leds, NUM_LEDS, FIRE_RED);
      if (elapsed >= 140) beginRound();
      return;
    }

    if (state == GAME_OVER) {
      clearLeds();
      uint8_t pulse = beatsin8(4, 30, 180);
      fill_solid(leds, NUM_LEDS, CRGB(pulse, 0, 0));
      uint8_t shown = displayedScore();
      for (uint8_t i = 0; i < NUM_LEDS; i++) {
        if (shown & (1U << i)) leds[i] = COLOR_CACTUS;
      }
    }
  }
};

AppFireball appFireball;

/* ================================================================
 * MENU HELPERS
 * ================================================================ */
void touchMenu() {
  menuLastActivity = millis();
}

bool inAnyMenu() {
  return uiState == UI_MENU_MAIN   ||
         uiState == UI_MENU_GAMES  ||
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
  uint8_t base = menuWindowBase(mainCursor, 7);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t item = base + i;
    if (item < 5) leds[i] = COLOR_CACTUS;
    else if (item == 5) leds[i] = COLOR_GAMES;
    else if (item == 6) leds[i] = COLOR_SETTINGS;
  }
  renderCursor(menuCursorLed(mainCursor, base), 180);
}

/* ================================================================
 * GAMES MENU
 * ================================================================ */
void renderGamesMenu() {
  clearLeds();
  leds[0] = CRGB(255, 100, 0);   // Back
  leds[1] = CRGB(255, 0, 0);     // Fireball
  renderCursor(gamesCursor, 180);
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
  renderCursor(menuCursorLed(settingsCursor, base), 180);
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
  clearLeds();
  FastLED.setBrightness(0);
  FastLED.show();

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
  reset_usb_boot(0, 0);
  while (true) delay(1000);
}

/* ================================================================
 * RETURN TO DEFAULT IDLE
 * ================================================================ */
void returnToIdle() {
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
    fill_solid(leds, NUM_LEDS, COLOR_HOT_PINK);
    FastLED.show();
    delay(120);
    returnToIdle();
    return;
  }

  // -------- FIREBALL --------
  if (uiState == UI_IN_GAME) {
    if (g == G_LONG_HOLD) {
      uiState = UI_MENU_GAMES;
      touchMenu();
      return;
    }
    appFireball.update(g);
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
  if (uiState == UI_MENU_MAIN) {
    if (g == G_TAP) {
      mainCursor = (uint8_t)((mainCursor + 1) % 7);
      return;
    }
    if (g == G_DOUBLE) {
      mainCursor = (mainCursor == 0) ? 6 : mainCursor - 1;
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
          currentMainMode = 1;
          modeFire.enter();
          uiState = UI_MODE;
          return;
        case 2:
          currentMainMode = 2;
          modeSaber.enter();
          uiState = UI_MODE;
          return;
        case 3:
          currentMainMode = 3;
          modeRainbow.enter();
          uiState = UI_MODE;
          return;
        case 4:
          uiState = UI_INVERTER;
          touchMenu();
          return;
        case 5:
          uiState = UI_MENU_GAMES;
          touchMenu();
          return;
        case 6:
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

  // -------- GAMES MENU --------
  if (uiState == UI_MENU_GAMES) {
    if (g == G_TAP) {
      gamesCursor = (uint8_t)((gamesCursor + 1) % 2);
      return;
    }
    if (g == G_DOUBLE) {
      gamesCursor = (gamesCursor == 0) ? 1 : 0;
      return;
    }
    if (g == G_HOLD) {
      if (gamesCursor == 0) {
        uiState = UI_MENU_MAIN;
        mainCursor = 5;
        touchMenu();
        return;
      }
      if (gamesCursor == 1) {
        appFireball.enter();
        uiState = UI_IN_GAME;
        touchMenu();
        return;
      }
    }
    if (g == G_LONG_HOLD) {
      uiState = UI_MENU_MAIN;
      mainCursor = 5;
      touchMenu();
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
        mainCursor = 6;
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
      mainCursor = 6;
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
      uiState = UI_MENU_MAIN;
      mainCursor = 4;
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
    case UI_MENU_GAMES:    renderGamesMenu();                    break;
    case UI_MENU_SETTINGS: renderSettingsMenu();                 break;
    case UI_BRIGHTNESS:    renderBrightness();                   break;
    case UI_SPEED:         renderSpeed();                        break;
    case UI_INFO:          renderInfo();                         break;
    case UI_INVERTER:      renderInverter();                    break;
    case UI_IN_GAME:       appFireball.render();                 break;
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
 *  Battery 11–14 V → Rsense1 → LM5122 boost → 17 V bus → UCC27735
 *  gate drivers → Q3/Q4/Q5/Q6 H-bridge → 12 V transformer winding →
 *  115 V outlet.  The MCU only generates PWM; the LM5122 runs in
 *  hardware.
 *
 *  Safety rules
 *  ------------
 *  - All four gate-driver inputs are driven low as the very first thing
 *    in setup() and are parked low whenever the bridge is not running.
 *  - The bridge is never enabled before the user explicitly arms it (the
 *    Inverter portal in the UI).  A valid voltage reading alone is not
 *    proof the hardware is safe to switch.
 *  - The bridge is never enabled before AIN1 confirms the 17 V bus.
 *  - Dead time is ≥ 500 ns on both edges by construction (phase-offset
 *    slices plus delayed inverted-channel compare).
 *  - Any fault latches POWER_FAULT and parks the gates low.  Non-critical
 *    faults can be cleared from the Inverter portal (double tap); over-current
 *    and over-temperature require an MCU reset.  Clearing disarms the bridge.
 *  - Sleep mode and BOOTSEL entry disarm the bridge before acting.
 *  - A hardware watchdog (≈ 200 ms) is fed from the main loop.  If the
 *    MCU resets, the UCC27735 input pull-downs bring the gates low.
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
 * ================================================================ */

// PWM constants.  150 MHz / 7500 = 20 kHz carrier; dt = 75 ticks = 500 ns.
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

// Divider ratios (bottom / (top + bottom)) from the schematic.
#define DIV_AIN0           (10.0f / (36.5f + 10.0f))   // input rail 11–14 V
#define DIV_AIN1           (10.0f / (43.2f + 10.0f))   // 17 V bus
#define DIV_AIN2           (10.0f / (45.3f + 10.0f))   // bridge midpoint A
#define DIV_AIN3           (10.0f / (45.3f + 10.0f))   // bridge midpoint B
#define INA240_V_PER_A     0.1f                         // AIN4, 0.1 V/A

// Transformer 115 V winding / 12 V winding.
#define TRANSFORMER_RATIO  (115.0f / 12.0f)
#define OUTPUT_TARGET_RMS  115.0f

// State-machine thresholds.
#define VIN_LOW            11.0f
#define VIN_HIGH           14.0f
#define VIN_HYST           0.2f
#define VBUS_NOMINAL       17.0f
#define VBUS_START_WINDOW  0.75f
#define VBUS_RUN_WINDOW    1.5f
#define IIN_WARN           15.0f
#define IIN_TRIP           20.0f
#define TEMP_WARN_C        60.0f
#define TEMP_TRIP_C        85.0f

// Control-loop timing.
#define CTRL_INTERVAL_MS   10     // ~100 Hz outer loop
#define RAMP_TIME_MS       1500   // soft-start ramp
#define M_START            0.30f  // modulation at the end of soft start
#define AMP_KI             0.0003f // integrator gain per 10 ms tick per volt of error

// RMS measurement cadence.
#define RMS_RING_SIZE      128
#define RMS_MIN_SAMPLES    96
#define RMS_SAMPLE_US      130

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

static float modulation = 0.0f;    // instantaneous m(t)
static float mTarget    = 0.0f;    // amplitude loop output
static float mRamp      = 0.0f;    // ramped setpoint during start
static uint32_t rampStartMs = 0;
static uint32_t lastCtrlMs  = 0;
static uint32_t sineOriginUs = 0;
static bool     sinePhaseStarted = false;

// RMS / output inference.
static float  rmsBuf[RMS_RING_SIZE];
static uint8_t rmsIndex = 0;
static uint8_t rmsCount = 0;
static float   outputRms = 0.0f;
static float   primaryDc = 0.0f;   // DC offset check
static uint32_t rmsLastSampleUs = 0;
static bool    rmsValid = false;

// ADS7128 scanning state.
#define ADS_CH_COUNT       5
static uint8_t  adsChannel = 0;
static bool     adsPresent = false;
static uint32_t adsLastConvUs = 0;
static uint32_t adsConvStartUs = 0;
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

static bool inputRailValid() {
  static bool wasValid = false;
  bool ok = (vIn >= VIN_LOW - (wasValid ? VIN_HYST : 0.0f)) &&
            (vIn <= VIN_HIGH + (wasValid ? VIN_HYST : 0.0f));
  wasValid = ok;
  return ok;
}

static bool busValidForStart() {
  return (vBus >= VBUS_NOMINAL - VBUS_START_WINDOW) &&
         (vBus <= VBUS_NOMINAL + VBUS_START_WINDOW);
}

static bool busValidForRun() {
  static bool wasValid = false;
  bool ok = (vBus >= VBUS_NOMINAL - VBUS_RUN_WINDOW) &&
            (vBus <= VBUS_NOMINAL + VBUS_RUN_WINDOW);
  if (!ok) {
    // Require the bus to be out of range persistently; single-sample noise
    // is ignored by checking again on the next control tick.
    if (wasValid) {
      wasValid = false;
      return true;   // one-cycle grace
    }
    return false;
  }
  wasValid = true;
  return true;
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

static bool adsProbe() {
  uint8_t status = 0;
  uint8_t cfg = 0;
  if (!adsReadReg(ADS7128_SYSTEM_STATUS, &status)) return false;
  // Force DATA_CFG to not append status bits, so reads are pure 16-bit codes.
  if (!adsWriteReg(ADS7128_DATA_CFG, 0x00)) return false;
  // RECENT_CHx only records while the statistics module is enabled
  // (datasheet §8.3.6 and §8.3.14); set STATS_EN once and read it back.
  if (!adsSetBits(ADS7128_GENERAL_CFG, 0x20)) return false;
  if (!adsReadReg(ADS7128_GENERAL_CFG, &cfg)) return false;
  return (cfg & 0x20) != 0;
}

static float adsCodeToVolts(uint16_t code) {
  return (float)code * 3.3f / 4096.0f;
}

static void adsStartConversion(uint8_t ch) {
  adsWriteReg(ADS7128_CHANNEL_SEL, ch & 0x0F);
  adsSetBits(ADS7128_GENERAL_CFG, 0x08);  // CNVST; STATS_EN is left set
  adsConvStartUs = micros();
  adsPhase = ADS_WAITING;
}

static void adsService() {
  if (!adsPresent) return;

  if (adsPhase == ADS_WAITING) {
    // Poll OSR_DONE (bit 3).  Typical conversion < 2 us; timeout 5 ms.
    uint8_t status = 0;
    if (!adsReadReg(ADS7128_SYSTEM_STATUS, &status)) {
      if (pArmed) setFault("ADS7128 NACK");
      else        adsPresent = false;
      return;
    }
    if (status & 0x08) {
      // OSR_DONE is sticky; write 1 to clear it so the next channel's poll
      // sees only the next conversion's completion (datasheet §8.6.1).
      if (!adsSetBits(ADS7128_SYSTEM_STATUS, 0x08)) {
        if (pArmed) setFault("ADS7128 NACK");
        else        adsPresent = false;
        return;
      }
      adsPhase = ADS_READING;
    } else if (micros() - adsConvStartUs > 5000) {
      if (pArmed) setFault("ADS7128 timeout");
      else        adsPresent = false;
    }
    return;
  }

  if (adsPhase == ADS_READING) {
    uint16_t code = 0;
    if (!adsReadResult(adsChannel, &code)) {
      if (pArmed) setFault("ADS7128 read");
      else        adsPresent = false;
      return;
    }
    float volts = adsCodeToVolts(code);
    switch (adsChannel) {
      case 0: vIn   = volts / DIV_AIN0; break;
      case 1: vBus  = volts / DIV_AIN1; break;
      case 2: vMidA = volts / DIV_AIN2; break;
      case 3: vMidB = volts / DIV_AIN3; break;
      case 4: iIn   = volts / INA240_V_PER_A; break;
    }
    if (adsChannel == 4) adsSwept = true;
    adsChannel = (adsChannel + 1) % ADS_CH_COUNT;
    adsPhase = ADS_IDLE;
    adsLastConvUs = micros();
  }

  if (adsPhase == ADS_IDLE) {
    // Throttle slightly to keep the loop responsive; each channel is still
    // read many times per mains cycle.
    if (micros() - adsLastConvUs >= 200) {
      adsStartConversion(adsChannel);
    }
  }
}

static float readTempAdc(uint8_t adcInput) {
  adc_select_input(adcInput);
  uint16_t raw = adc_read();
  float volts = (float)raw * 3.3f / 4096.0f;
  return (volts - 0.5f) / 0.01f;
}

static void readTemperatures() {
  // Sample each sensor occasionally; TMP235 is slow and thermal mass is large.
  static uint8_t tempPhase = 0;
  static uint32_t lastTempMs = 0;
  uint32_t now = millis();
  if (now - lastTempMs < 100) return;
  lastTempMs = now;
  if (tempPhase == 0) tempCtrl = readTempAdc(0);
  else                tempInv  = readTempAdc(1);
  tempPhase ^= 1;
}

static void updateRmsAndDc() {
  uint32_t nowUs = micros();
  if (nowUs - rmsLastSampleUs < RMS_SAMPLE_US) return;
  rmsLastSampleUs = nowUs;

  float diff = vMidA - vMidB;            // transformer primary differential
  rmsBuf[rmsIndex] = diff;
  rmsIndex = (uint8_t)((rmsIndex + 1) % RMS_RING_SIZE);
  if (rmsCount < RMS_RING_SIZE) rmsCount++;

  if (rmsCount < RMS_MIN_SAMPLES) {
    rmsValid = false;
    return;
  }
  rmsValid = true;

  // DC offset over the same window.
  float sum = 0.0f;
  for (uint8_t i = 0; i < rmsCount; i++) sum += rmsBuf[i];
  primaryDc = sum / (float)rmsCount;

  // RMS of primary differential; outlet RMS scales by transformer ratio.
  float sq = 0.0f;
  for (uint8_t i = 0; i < rmsCount; i++) {
    float d = rmsBuf[i] - primaryDc;
    sq += d * d;
  }
  float primaryRms = sqrtf(sq / (float)rmsCount);
  outputRms = primaryRms * TRANSFORMER_RATIO;
}

static void writeBridgeDuty(float m) {
  // m is the modulation index, clamped so duty stays inside dead-time band.
  m = clampf(m, -PWM_M_MAX, PWM_M_MAX);

  float dA = (1.0f + m) * 0.5f;
  float dB = (1.0f - m) * 0.5f;

  // Convert to PWM compare values (integer ticks).
  uint16_t ccAhi = (uint16_t)(dA * (float)(PWM_TOP + 1));
  uint16_t ccAlo = (uint16_t)(dA * (float)(PWM_TOP + 1) + 2.0f * (float)PWM_DT_TICKS);
  uint16_t ccBhi = (uint16_t)(dB * (float)(PWM_TOP + 1));
  uint16_t ccBlo = (uint16_t)(dB * (float)(PWM_TOP + 1) + 2.0f * (float)PWM_DT_TICKS);

  if (ccAlo > PWM_TOP) ccAlo = PWM_TOP;
  if (ccBlo > PWM_TOP) ccBlo = PWM_TOP;
  if (ccAhi < 2 * PWM_DT_TICKS) ccAhi = 2 * PWM_DT_TICKS;
  if (ccBhi < 2 * PWM_DT_TICKS) ccBhi = 2 * PWM_DT_TICKS;

  pwm_set_chan_level(PWM_SLICE_A_HI, PWM_CHAN_A_HI, ccAhi);
  pwm_set_chan_level(PWM_SLICE_A_LO, PWM_CHAN_A_LO, ccAlo);
  pwm_set_chan_level(PWM_SLICE_B_HI, PWM_CHAN_B_HI, ccBhi);
  pwm_set_chan_level(PWM_SLICE_B_LO, PWM_CHAN_B_LO, ccBlo);
}

static void stopBridge() {
  parkGatesLow();
  modulation = 0.0f;
  mTarget = 0.0f;
  mRamp = 0.0f;
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
}

static void startBridge() {
  if (pState == POWER_FAULT) return;
  if (!pArmed) return;
  if (!sensorsOk) return;
  if (!busValidForStart()) return;
  enablePwmSlices();
  rampStartMs = millis();
  mRamp = 0.0f;
  mTarget = 0.0f;
  sineOriginUs = micros();
  sinePhaseStarted = true;
  pState = POWER_STARTING;
}

static void runControl(uint32_t nowMs) {
  if (nowMs - lastCtrlMs < CTRL_INTERVAL_MS) return;
  lastCtrlMs = nowMs;

  // Warnings (do not trip).
  static bool warnedCurrent = false;
  static bool warnedTemp = false;
  if (iIn >= IIN_WARN && !warnedCurrent) {
    warnedCurrent = true;
    if (Serial) Serial.println("WARN: input current");
  } else if (iIn < IIN_WARN - 1.0f) {
    warnedCurrent = false;
  }
  if (((tempCtrl >= TEMP_WARN_C) || (tempInv >= TEMP_WARN_C)) && !warnedTemp) {
    warnedTemp = true;
    if (Serial) Serial.println("WARN: temperature");
  } else if ((tempCtrl < TEMP_WARN_C - 5.0f) && (tempInv < TEMP_WARN_C - 5.0f)) {
    warnedTemp = false;
  }

  // Protections are only evaluated while the bridge is armed.  A voltage
  // reading alone is not proof the hardware is safe to switch.
  if (pArmed) {
    if (iIn >= IIN_TRIP)      { setFault("over-current"); return; }
    if (tempCtrl >= TEMP_TRIP_C) { setFault("over-temp CTRL"); return; }
    if (tempInv  >= TEMP_TRIP_C) { setFault("over-temp INV"); return; }
    if (!inputRailValid())    { setFault("input rail"); return; }

    if (pState == POWER_RUNNING || pState == POWER_STARTING) {
      if (!busValidForRun()) { setFault("bus voltage"); return; }

      // Plausibility: output must be roughly proportional to modulation.
      if (rmsValid) {
        if (outputRms > OUTPUT_TARGET_RMS * 1.25f) {
          setFault("output overvoltage");
          return;
        }
        if (mRamp > 0.05f && outputRms < OUTPUT_TARGET_RMS * 0.10f) {
          setFault("output missing");
          return;
        }
        // DC offset on the primary implies a DC component across the transformer.
        // ASSUMPTION: 2 V DC-offset threshold; verify with scope/data before first
        // energised test.
        if (fabsf(primaryDc) > 2.0f) {
          setFault("DC offset");
          return;
        }
      }
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
  if (pState == POWER_RUNNING && rmsValid) {
    float error = OUTPUT_TARGET_RMS - outputRms;
    mTarget += error * AMP_KI;
    mTarget = clampf(mTarget, 0.0f, PWM_M_MAX);
    mRamp = mTarget;
  }

  modulation = mRamp;
  if (modulation > PWM_M_MAX) modulation = PWM_M_MAX;
  if (modulation < 0.0f) modulation = 0.0f;
}

static void updateSineModulation(uint32_t nowUs) {
  if (pState != POWER_STARTING && pState != POWER_RUNNING) return;

  // 60 Hz phase, continuous in time so phase is not disturbed by loop jitter.
  // ASSUMPTION: float sinf is acceptable here because this runs once per main
  // loop (kHz-rate), not once per 20 kHz PWM carrier period.
  if (!sinePhaseStarted) {
    sineOriginUs = nowUs;
    sinePhaseStarted = true;
  }
  uint32_t dt = nowUs - sineOriginUs;
  float theta = 2.0f * (float)PI * 60.0f * ((float)dt / 1.0e6f);
  float m = modulation * sinf(theta);
  writeBridgeDuty(m);
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

  // I2C for ADS7128 at 400 kHz, fall back to 100 kHz.
  Wire.setSDA(8);
  Wire.setSCL(9);
  Wire.begin();
  Wire.setClock(400000);
  adsPresent = adsProbe();
  if (!adsPresent) {
    Wire.setClock(100000);
    adsPresent = adsProbe();
  }
  if (!adsPresent) {
    if (Serial) Serial.println("WARN: ADS7128 not found at boot");
  }

  // RP2350 ADC for TMP235 temperature sensors.
  adc_init();
  adc_gpio_init(TEMP_CTRL);
  adc_gpio_init(TEMP_INV);

  // Hardware watchdog.  Safe because the UCC27735 inputs have internal
  // pull-downs that bring the gates low while the MCU is in reset.
  watchdog_enable(200, true);
}

void powerService(uint32_t nowMs) {
  watchdog_update();

  // Throttled ADS7128 probe retry (only while disarmed / not faulted).
  static uint32_t lastProbeMs = 0;
  if (!adsPresent && !pFault && (nowMs - lastProbeMs >= 500)) {
    lastProbeMs = nowMs;
    if (Wire.setClock(400000), adsProbe()) {
      adsPresent = true;
      Wire.setClock(400000);
    } else {
      Wire.setClock(100000);
      adsPresent = adsProbe();
    }
  }

  adsService();
  readTemperatures();
  updateRmsAndDc();
  runControl(nowMs);
  updateSineModulation(micros());

  // sensorsOk = probe succeeded AND at least one full sweep completed.
  sensorsOk = adsPresent && adsSwept;

  // Disarmed → never leave IDLE.
  if (!pArmed) {
    if (pState != POWER_FAULT) pState = POWER_IDLE;
    if (pState == POWER_IDLE) stopBridge();
  }

  // State transitions (armed only).
  if (pArmed && !pFault) {
    if (pState == POWER_IDLE) {
      if (sensorsOk && inputRailValid()) {
        pState = POWER_WAIT_BUS;
        if (Serial) Serial.println("STATE: WAIT_BUS");
      }
    } else if (pState == POWER_WAIT_BUS) {
      if (!inputRailValid()) {
        pState = POWER_IDLE;
      } else if (busValidForStart()) {
        startBridge();
        if (Serial) Serial.println("STATE: STARTING");
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
float powerOutputVolts() { return outputRms; }
float powerOutputAmps()  { return iIn; }

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
  // Over-current and over-temperature require a physical reset for safety.
  if (strncmp(pFaultReason, "over-", 5) == 0) {
    if (Serial) Serial.println("CLEAR ignored: over-fault latched");
    return;
  }
  pFault = false;
  pArmed = false;
  pState = POWER_IDLE;
  memset(pFaultReason, 0, sizeof(pFaultReason));
  stopBridge();
  if (Serial) Serial.println("Fault cleared (disarmed)");
}

bool powerArmed() { return pArmed; }

bool powerSensorsOk() { return sensorsOk; }

bool powerSetArmed(bool on) {
  if (!on) {
    pArmed = false;
    stopBridge();
    if (pState != POWER_FAULT) pState = POWER_IDLE;
    if (Serial) Serial.println("DISARMED");
    return true;
  }
  if (pFault) {
    if (Serial) Serial.println("ARM refused: fault latched");
    return false;
  }
  if (!sensorsOk) {
    if (Serial) Serial.println("ARM refused: no sensors");
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
  Serial.print(" VIN=");   Serial.print(vIn);
  Serial.print(" VBUS=");  Serial.print(vBus);
  Serial.print(" VOUT=");  Serial.print(outputRms);
  Serial.print(" IIN=");   Serial.print(iIn);
  Serial.print(" Tctrl="); Serial.print(tempCtrl);
  Serial.print(" Tinv=");  Serial.print(tempInv);
  Serial.print(" m=");     Serial.println(modulation, 3);
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
  }
  delay(70);
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

  if (uiState == UI_SLEEP) {
    updateShutdown();
    delay(5);
    return;
  }

  entropy.update();

  if (inAnyMenu() && now - menuLastActivity > MENU_TIMEOUT_MS) {
    returnToIdle();
  }

  Gesture g = button.poll(inAnyMenu() || uiState == UI_IN_GAME);
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
  if (st == 1) { // POWER_WAIT_BUS
    uint8_t p = beatsin8(4, 30, 180);
    leds[0] = CRGB(p, p / 2, 0);
    return;
  }

  if (st == 2) { // POWER_STARTING
    float v = powerBusVolts();
    float frac = v / 17.0f;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    uint8_t lit = (uint8_t)(frac * NUM_LEDS);
    if (lit > NUM_LEDS) lit = NUM_LEDS;
    for (uint8_t i = 0; i < lit; i++) leds[i] = COLOR_CACTUS;
    if (lit < NUM_LEDS) leds[lit] = CRGB(0, 40, 10);
    return;
  }

  if (st == 3) { // POWER_RUNNING
    float frac = powerOutputVolts() / 115.0f;
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
