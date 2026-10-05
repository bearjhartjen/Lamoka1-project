/*
 * ================================================================
 * LAMOKA1 DEV BRAIN — Portal Interface Engine V1.2
 * ================================================================
 *
 * Major changes:
 *   - Lightsaber:
 *       * New random color every launch
 *       * Random launch animation/pattern every launch
 *       * Random instability/flicker
 *       * Random retract behavior
 *
 *   - Fireplace:
 *       * Heat diffusion model
 *       * Upward convection
 *       * Cooling
 *       * Random ignition
 *       * Ember base
 *       * Natural flame flicker
 *
 *   - Fireball:
 *       * Uses a diffused fire/heat model
 *       * Bright moving fireball head
 *       * Diffused trail
 *       * Heat distortion/randomness
 *       * Existing lives + binary score retained
 *
 *   - Settings:
 *       * Brightness
 *       * Speed
 *       * Info
 *       * Shutdown
 *
 *   - Shutdown:
 *       * Turns all LEDs completely off
 *       * Ignores normal button commands
 *       * Requires a sustained button hold to wake
 *
 * NOTE:
 * Arduino software cannot physically remove power from the RP2350
 * unless the board has a hardware power-latch / MOSFET controlled
 * shutdown circuit. This implementation is therefore a software
 * power-down state.
 * ================================================================
 */

#include <Arduino.h>
#include <FastLED.h>

/* ================================================================
 * HARDWARE
 * ================================================================ */

#define LED_PIN            11
#define NUM_LEDS           10
#define BUTTON_PIN         0

#define COLOR_ENTROPY_PIN  5

#define ADC0               26
#define ADC1               27
#define ADC2               28
#define ADC3               29

#define DEFAULT_BRIGHTNESS 36
#define DEFAULT_SPEED      100

#define FRAME_MS           16
#define MENU_TIMEOUT_MS    16000UL

CRGB leds[NUM_LEDS];

/* ================================================================
 * GESTURES
 * ================================================================ */

enum Gesture : uint8_t {
  G_NONE = 0,
  G_TAP,
  G_DOUBLE,
  G_HOLD,
  G_LONG_HOLD,
  G_VERY_LONG
};

/* ================================================================
 * UI STATES
 * ================================================================ */

enum UIState : uint8_t {
  UI_MODE = 0,
  UI_MENU_MAIN,
  UI_MENU_APPS,
  UI_MENU_SETTINGS,
  UI_BRIGHTNESS,
  UI_SPEED,
  UI_INFO,
  UI_IN_APP,
  UI_SLEEP
};

/* ================================================================
 * GLOBAL SETTINGS
 * ================================================================ */

uint8_t globalBrightness = DEFAULT_BRIGHTNESS;
uint8_t globalSpeed      = DEFAULT_SPEED;

void applyBrightness() {
  uint8_t b = globalBrightness;

  // Keep menus visible.
  if (b < 18) b = 18;

  FastLED.setBrightness(b);
}

void clearLeds() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
}

CRGB blendColor(const CRGB& a, const CRGB& b, uint8_t t) {
  return CRGB(
    lerp8by8(a.r, b.r, t),
    lerp8by8(a.g, b.g, t),
    lerp8by8(a.b, b.b, t)
  );
}

/* ================================================================
 * ENTROPY SERVICE
 * ================================================================ */

class EntropyService {
public:

  void begin() {
    pinMode(ADC0, INPUT);
    pinMode(ADC1, INPUT);
    pinMode(ADC2, INPUT);
    pinMode(ADC3, INPUT);

    pinMode(COLOR_ENTROPY_PIN, INPUT);

    state = 0xA341316CUL ^ micros();

    for (int i = 0; i < 400; i++) {
      mix();
    }
  }

  void update() {
    mix();
  }

  uint32_t next() {
    mix();

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    return state;
  }

  uint8_t next8() {
    return next() & 0xFF;
  }

  uint16_t range(uint16_t maxValue) {
    if (!maxValue) return 0;
    return next() % (maxValue + 1);
  }

  bool coin() {
    return next() & 1;
  }

private:

  uint32_t state = 1;

  void mix() {
    uint32_t a = analogRead(ADC0);
    uint32_t b = analogRead(ADC1);
    uint32_t c = analogRead(ADC2);
    uint32_t d = analogRead(ADC3);

    uint32_t t = micros();
    uint32_t p = digitalRead(COLOR_ENTROPY_PIN);

    state ^= (a << 16) ^ b;
    state ^= (c << 8) ^ d;
    state ^= t;
    state ^= p << 23;

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    state += 0x9E3779B9UL;
  }
};

EntropyService entropy;

/* ================================================================
 * BUTTON SERVICE
 * ================================================================ */

class ButtonService {
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

    if (now - debounceStart < 28) {
      return G_NONE;
    }

    bool pressed = !raw;

    /* Rising edge into pressed state */

    if (pressed && !wasPressed) {

      wasPressed = true;
      pressStart = now;

      holdFired = false;
      longFired = false;
      veryLongFired = false;

      return G_NONE;
    }

    /* Holding */

    if (pressed) {

      uint32_t h = now - pressStart;

      if (h >= 2800 && !veryLongFired) {
        veryLongFired = true;
        return G_VERY_LONG;
      }

      if (h >= 1200 && !longFired) {
        longFired = true;
        return G_LONG_HOLD;
      }

      if (h >= 550 && !holdFired) {
        holdFired = true;
        return G_HOLD;
      }
    }

    /* Release */

    if (!pressed && wasPressed) {

      wasPressed = false;

      uint32_t h = now - pressStart;

      if (!holdFired &&
          !longFired &&
          !veryLongFired &&
          h < 400) {

        /* Menus use simple taps immediately. */

        if (inMenu) {
          return G_TAP;
        }

        /* Outside menus we need to differentiate tap/double tap. */

        if (lastTap && now - lastTap < 380) {

          lastTap = 0;
          pending = false;

          return G_DOUBLE;
        }

        lastTap = now;
        pending = true;
      }
    }

    /* Single tap timeout */

    if (!inMenu &&
        pending &&
        lastTap &&
        now - lastTap >= 380) {

      pending = false;

      return G_TAP;
    }

    return G_NONE;
  }

private:

  bool lastRaw = HIGH;
  bool wasPressed = false;
  bool pending = false;

  bool holdFired = false;
  bool longFired = false;
  bool veryLongFired = false;

  uint32_t pressStart = 0;
  uint32_t debounceStart = 0;
  uint32_t lastTap = 0;
};

ButtonService button;

/* ================================================================
 * FIRE COLOR HELPER
 * ================================================================ */

/*
 * Convert heat value into a more natural flame:
 *
 *   low heat    = deep red / ember
 *   medium      = red / orange
 *   high        = orange / yellow
 *   extreme     = hot yellow / white
 */

CRGB flameColor(uint8_t heat) {

  if (heat < 32) {

    return blendColor(
      CRGB(10, 0, 0),
      CRGB(55, 2, 0),
      scale8(heat, 255)
    );
  }

  if (heat < 80) {

    return blendColor(
      CRGB(55, 2, 0),
      CRGB(150, 12, 0),
      scale8(heat - 32, 255)
    );
  }

  if (heat < 150) {

    return blendColor(
      CRGB(150, 12, 0),
      CRGB(255, 65, 0),
      scale8(heat - 80, 255)
    );
  }

  if (heat < 220) {

    return blendColor(
      CRGB(255, 65, 0),
      CRGB(255, 165, 25),
      scale8(heat - 150, 255)
    );
  }

  return blendColor(
    CRGB(255, 165, 25),
    CRGB(255, 240, 145),
    scale8(heat - 220, 255)
  );
}

/* ================================================================
 * GENERIC MODE
 * ================================================================ */

class Mode {
public:

  virtual ~Mode() {}

  virtual void enter() {}

  virtual void update(Gesture) {}

  virtual void render() {}
};

/* ================================================================
 * STATUS MODE
 * ================================================================ */

class ModeStatus : public Mode {
public:

  void enter() override {
    pos = 0;
    dir = 1;
    last = millis();
  }

  void render() override {

    clearLeds();

    uint16_t interval =
      28 + 80 * 100 / (globalSpeed + 50);

    if (millis() - last >= interval) {

      last = millis();

      pos += dir;

      if (pos >= 9) {
        pos = 9;
        dir = -1;
      }

      if (pos <= 0) {
        pos = 0;
        dir = 1;
      }
    }

    for (int t = 0; t < 3; t++) {

      int p = pos - dir * t;

      if (p >= 0 && p < NUM_LEDS) {

        leds[p] = CRGB(
          0,
          25 + t * 35,
          70 + t * 55
        );
      }
    }

    leds[pos] = CRGB(0, 70, 255);
  }

private:

  int8_t pos = 0;
  int8_t dir = 1;

  uint32_t last = 0;
};

/* ================================================================
 * FIREPLACE MODE
 *
 * Heat travels upward from the base.
 *
 * Each frame:
 *   1. cools
 *   2. diffuses between neighboring cells
 *   3. injects new heat at the bottom
 *   4. adds random flame impulses
 *
 * This gives significantly more natural behavior than simply
 * assigning random RGB values to each LED.
 * ================================================================ */

class ModeFire : public Mode {

public:

  void enter() override {

    memset(heat, 0, sizeof(heat));

    randomSeed(entropy.next());

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      heat[i] = entropy.next8() / 4;
    }
  }

  void update(Gesture g) override {

    if (g == G_TAP) {

      /*
       * Tap throws extra fuel into the fire.
       */
      for (uint8_t i = 0; i < 3; i++) {
        heat[i] = qadd8(heat[i], entropy.range(90));
      }
    }

    if (g == G_DOUBLE) {

      /*
       * Double tap causes a strong flare.
       */
      for (uint8_t i = 0; i < NUM_LEDS; i++) {
        heat[i] = qadd8(heat[i], 45 + entropy.range(90));
      }
    }
  }

  void render() override {

    /*
     * Flame evolution speed.
     */
    uint8_t evolution =
      2 + (globalSpeed / 28);

    if (evolution > 9) {
      evolution = 9;
    }

    /*
     * 1. COOLING
     *
     * Higher cells generally cool more.
     */
    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t cooling =
        2 +
        entropy.range(7) +
        (i * 2);

      if (cooling > heat[i]) {
        heat[i] = 0;
      } else {
        heat[i] -= cooling;
      }
    }

    /*
     * 2. UPWARD DIFFUSION
     *
     * Hot lower cells influence the cells above them.
     */
    for (int i = NUM_LEDS - 1; i >= 2; i--) {

      uint16_t avg =
        (uint16_t)heat[i - 1] +
        (uint16_t)heat[i - 2] +
        (uint16_t)heat[i - 3];

      avg /= 3;

      uint8_t smoothing =
        150 + (evolution * 8);

      heat[i] =
        scale8(
          avg,
          smoothing
        );
    }

    /*
     * 3. FIREBASE
     *
     * Bottom of flame constantly receives fuel.
     */
    uint8_t baseFuel =
      130 +
      entropy.range(100);

    if (globalSpeed > 150) {
      baseFuel += 15;
    }

    heat[0] = qadd8(heat[0], baseFuel);

    if (entropy.next8() > 80) {
      heat[1] =
        qadd8(
          heat[1],
          25 + entropy.range(80)
        );
    }

    /*
     * 4. RANDOM FLAME LICKS
     *
     * Occasional pockets of hotter gas rise through
     * the flame.
     */
    if (entropy.next8() > 192) {

      uint8_t start =
        1 + entropy.range(4);

      heat[start] =
        qadd8(
          heat[start],
          80 + entropy.range(100)
        );

      if (start + 1 < NUM_LEDS) {

        heat[start + 1] =
          qadd8(
            heat[start + 1],
            35 + entropy.range(60)
          );
      }
    }

    /*
     * 5. ADD A SLIGHT SIDE-TO-SIDE FLAME MOTION
     *
     * This keeps the flame from becoming a static vertical column.
     */
    uint8_t wind =
      inoise8(millis() / 14);

    for (uint8_t i = 2; i < NUM_LEDS; i++) {

      uint8_t drift =
        inoise8(
          millis() / 18 +
          i * 33
        );

      if ((wind + drift) & 0x20) {

        heat[i] =
          scale8(
            heat[i],
            238
          );
      }
    }

    /*
     * 6. DRAW
     */
    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t h = heat[i];

      /*
       * Ember base.
       */
      if (i == 0 && h < 55) {
        h = 55;
      }

      CRGB c = flameColor(h);

      /*
       * Tiny flame flicker.
       */
      if (h > 80 &&
          entropy.next8() > 225) {

        c.nscale8(180);
      }

      leds[i] = c;
    }
  }

private:

  uint8_t heat[NUM_LEDS];
};

/* ================================================================
 * LIGHTSABER
 *
 * Every launch gets:
 *   - new hue
 *   - new saturation
 *   - new animation style
 *   - new instability level
 *
 * Launch patterns:
 *
 *   0 = normal linear ignition
 *   1 = explosive ignition
 *   2 = bottom-up pulse
 *   3 = unstable flicker
 *   4 = scanning ignition
 *   5 = crackling ignition
 * ================================================================ */

class ModeSaber : public Mode {

public:

  void enter() override {

    hue = entropy.next8();

    saturation =
      175 +
      entropy.range(65);

    pattern =
      entropy.range(5);

    instability =
      20 +
      entropy.range(75);

    power = 0;

    extending = true;

    ignitionTime = millis();

    lastLaunch = millis();

    pulse = entropy.next8();

    clash = 0;
  }

  void update(Gesture g) override {

    if (g == G_TAP) {

      /*
       * Toggle saber.
       *
       * A rising launch ALWAYS picks a fresh color/pattern.
       */
      if (!extending) {

        launchRandom();
      } else {

        extending = false;
      }
    }

    if (g == G_DOUBLE) {

      /*
       * Force a completely new saber configuration.
       */
      launchRandom();
    }

    if (g == G_HOLD) {

      /*
       * Saber clash.
       */
      clash = 16;
    }
  }

  void render() override {

    clearLeds();

    uint32_t now = millis();

    if (extending) {

      /*
       * Speed of extension varies by pattern.
       */
      uint8_t launchRate =
        7 +
        (globalSpeed / 30);

      if (pattern == 1) launchRate += 4;
      if (pattern == 3) launchRate -= 1;
      if (launchRate < 2) launchRate = 2;

      power =
        qadd8(
          power,
          launchRate
        );
    } else {

      /*
       * Retract faster than extension.
       */
      power =
        qsub8(
          power,
          10
        );
    }

    uint8_t visibleLength =
      scale8(
        power,
        NUM_LEDS
      );

    if (visibleLength > NUM_LEDS) {
      visibleLength = NUM_LEDS;
    }

    /*
     * Pattern-specific behavior.
     */

    for (uint8_t i = 0; i < visibleLength; i++) {

      uint8_t brightness = 255;

      /*
       * Main blade brightness.
       */
      brightness =
        qsub8(
          255,
          i * 11
        );

      /*
       * UNSTABLE PATTERN
       */
      if (pattern == 3) {

        uint8_t flicker =
          entropy.next8();

        if (flicker < instability) {
          brightness = scale8(brightness, 120);
        }
      }

      /*
       * CRACKLING PATTERN
       */
      if (pattern == 5) {

        if (entropy.next8() > 220) {
          brightness = 255;
        }
      }

      /*
       * SCANNING PATTERN
       */
      if (pattern == 4) {

        uint8_t scan =
          beatsin8(
            12,
            145,
            255
          );

        brightness =
          scale8(
            brightness,
            scan
          );
      }

      /*
       * EXPLOSIVE PATTERN
       */
      if (pattern == 1 &&
          i == 0) {

        brightness = 255;
      }

      /*
       * PULSE PATTERN
       */
      if (pattern == 2) {

        uint8_t p =
          beatsin8(
            9,
            185,
            255
          );

        brightness =
          scale8(
            brightness,
            p
          );
      }

      leds[i] =
        CHSV(
          hue,
          saturation,
          brightness
        );

      /*
       * White-hot saber core.
       */
      uint8_t coreAmount = 75;

      if (pattern == 1) {
        coreAmount = 95;
      }

      if (i < 2) {

        leds[i] =
          blendColor(
            leds[i],
            CRGB::White,
            coreAmount
          );
      }
    }

    /*
     * Launch flashes.
     */

    if (extending &&
        now - ignitionTime < 170) {

      if (pattern == 1) {

        /*
         * Explosive launch.
         */
        for (uint8_t i = 0; i < NUM_LEDS; i++) {

          if (entropy.next8() > 120) {

            leds[i] =
              blendColor(
                leds[i],
                CRGB::White,
                120
              );
          }
        }

      } else {

        /*
         * Normal white ignition core.
         */
        if (visibleLength > 0) {

          leds[0] =
            blendColor(
              leds[0],
              CRGB::White,
              180
            );
        }
      }
    }

    /*
     * Clash effect.
     */
    if (clash) {

      for (uint8_t i = 0; i < visibleLength; i++) {

        if (entropy.next8() > 90) {

          leds[i] =
            blendColor(
              leds[i],
              CRGB::White,
              210
            );
        }
      }

      clash--;
    }

    /*
     * Tiny randomized blade sparkle.
     */
    if (visibleLength > 0) {

      uint8_t sparkle =
        entropy.next8();

      if (sparkle > 220) {

        uint8_t p =
          sparkle % visibleLength;

        leds[p] =
          blendColor(
            leds[p],
            CRGB::White,
            150
          );
      }
    }

    if (power == 0 && !extending) {
      clearLeds();
    }
  }

private:

  uint8_t hue = 0;
  uint8_t saturation = 220;

  uint8_t pattern = 0;
  uint8_t instability = 30;

  uint8_t power = 0;
  uint8_t clash = 0;

  bool extending = true;

  uint8_t pulse = 0;

  uint32_t ignitionTime = 0;
  uint32_t lastLaunch = 0;

  void launchRandom() {

    hue =
      entropy.next8();

    saturation =
      175 +
      entropy.range(65);

    pattern =
      entropy.range(6);

    instability =
      20 +
      entropy.range(80);

    pulse =
      entropy.next8();

    extending = true;

    ignitionTime = millis();

    power = 0;

    lastLaunch = millis();
  }
};

/* ================================================================
 * RAINBOW
 * ================================================================ */

class ModeRainbow : public Mode {

public:

  void update(Gesture g) override {

    if (g == G_TAP) {
      speedMode =
        (speedMode % 10) + 1;
    }

    if (g == G_DOUBLE) {
      reverse = !reverse;
    }
  }

  void render() override {

    uint8_t step =
      speedMode *
      (globalSpeed + 40) /
      75;

    if (!step) step = 1;

    if (reverse) {
      hue -= step;
    } else {
      hue += step;
    }

    fill_rainbow(
      leds,
      NUM_LEDS,
      hue,
      16
    );
  }

private:

  uint8_t hue = 0;
  uint8_t speedMode = 3;
  bool reverse = false;
};

/* ================================================================
 * PULSE
 * ================================================================ */

class ModePulse : public Mode {

public:

  void update(Gesture g) override {

    if (g == G_TAP) {
      hue += 30;
    }

    if (g == G_DOUBLE) {
      rate = (rate % 8) + 2;
    }
  }

  void render() override {

    uint8_t r =
      rate *
      (globalSpeed + 45) /
      95;

    if (r < 2) r = 2;

    fill_solid(
      leds,
      NUM_LEDS,
      CHSV(
        hue,
        205,
        beatsin8(
          r,
          35,
          255
        )
      )
    );
  }

private:

  uint8_t hue = 200;
  uint8_t rate = 4;
};

/* ================================================================
 * ENTROPY MODE
 * ================================================================ */

class ModeEntropy : public Mode {

public:

  void update(Gesture g) override {

    if (g == G_TAP) {
      speedAmount =
        (speedAmount + 28) % 150 + 25;
    }

    if (g == G_DOUBLE) {
      palette =
        (palette + 1) % 3;
    }
  }

  void render() override {

    timeValue +=
      speedAmount *
      (globalSpeed + 40) /
      85;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t n =
        inoise8(
          timeValue + i * 42
        );

      if (palette == 0) {

        leds[i] =
          CHSV(
            n,
            215,
            210
          );

      } else if (palette == 1) {

        leds[i] =
          CHSV(
            155 + (n >> 2),
            175,
            n
          );

      } else {

        leds[i] =
          CRGB(
            n,
            n / 2,
            255 - n
          );
      }
    }
  }

private:

  uint16_t timeValue = 0;
  uint8_t speedAmount = 55;
  uint8_t palette = 0;
};

/* ================================================================
 * BINARY
 * ================================================================ */

class ModeBinary : public Mode {

public:

  void enter() override {

    value = 0;
    bit = 0;
  }

  void update(Gesture g) override {

    if (g == G_TAP) {

      value ^=
        (1 << bit);

      bit =
        (bit + 1) % NUM_LEDS;
    }

    if (g == G_DOUBLE) {

      value =
        entropy.next() &
        0x3FF;
    }

    if (g == G_HOLD) {

      value = 0;
    }
  }

  void render() override {

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      if (value & (1 << i)) {

        leds[i] =
          CRGB(
            0,
            180,
            255
          );

      } else {

        leds[i] =
          CRGB(
            8,
            6,
            14
          );
      }
    }

    if ((millis() / 240) & 1) {

      leds[bit] =
        blendColor(
          leds[bit],
          CRGB::White,
          160
        );
    }
  }

private:

  uint16_t value = 0;
  uint8_t bit = 0;
};

/* ================================================================
 * AMBIENT
 * ================================================================ */

class ModeAmbient : public Mode {

public:

  void render() override {

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t n =
        inoise8(
          millis() / 7 +
          i * 22
        );

      leds[i] =
        CHSV(
          145 + (n >> 3),
          170,
          35 + (n >> 2)
        );
    }
  }
};

/* ================================================================
 * DIFFUSED FIREBALL ENGINE
 *
 * The actual fireball uses a tiny heat field instead of just drawing
 * a red/orange dot. That makes the head and its trail diffuse into
 * each other like a real hot object moving through the flame.
 * ================================================================ */

class FireballFireEngine {

public:

  void reset() {

    memset(heat, 0, sizeof(heat));

    position = NUM_LEDS - 1;

    fractional = 0.0f;

    seed = entropy.next();
  }

  void update(float positionIn) {

    position = positionIn;

    /*
     * Cool the entire field.
     */
    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t cool =
        8 +
        entropy.range(10);

      if (cool > heat[i]) {
        heat[i] = 0;
      } else {
        heat[i] -= cool;
      }
    }

    /*
     * Diffuse hot material.
     */
    for (int i = NUM_LEDS - 1; i >= 2; i--) {

      uint16_t average =
        (uint16_t)heat[i - 1] +
        (uint16_t)heat[i - 2] +
        (uint16_t)heat[i - 1];

      average /= 3;

      heat[i] =
        scale8(
          average,
          205
        );
    }

    /*
     * Inject a moving hot core.
     */
    int head =
      (int)round(position);

    if (head >= 0 &&
        head < NUM_LEDS) {

      heat[head] =
        qadd8(
          heat[head],
          235
        );

      if (head + 1 < NUM_LEDS) {

        heat[head + 1] =
          qadd8(
            heat[head + 1],
            150
          );
      }

      if (head - 1 >= 0) {

        heat[head - 1] =
          qadd8(
            heat[head - 1],
            100
          );
      }
    }

    /*
     * Diffuse again to create a glowing ball around the head.
     */
    for (uint8_t i = 1; i < NUM_LEDS - 1; i++) {

      uint16_t local =
        (uint16_t)heat[i - 1] +
        (uint16_t)heat[i] +
        (uint16_t)heat[i + 1];

      local /= 3;

      heat[i] =
        qadd8(
          heat[i],
          scale8(
            local,
            70
          )
        );
    }

    seed +=
      27 +
      entropy.range(50);
  }

  void render() {

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t h = heat[i];

      if (!h) {

        leds[i] =
          CRGB::Black;

        continue;
      }

      CRGB c =
        flameColor(h);

      /*
       * Tiny random transparency / diffusion variation.
       */
      uint8_t noise =
        inoise8(
          seed +
          i * 37
        );

      if (noise < 60) {

        c.nscale8(
          175
        );
      }

      leds[i] = c;
    }

    /*
     * Bright white-yellow fireball head.
     */
    int head =
      (int)round(position);

    if (head >= 0 &&
        head < NUM_LEDS) {

      CRGB headColor =
        CRGB(
          255,
          225,
          85
        );

      leds[head] =
        blendColor(
          leds[head],
          headColor,
          220
        );
    }
  }

private:

  uint8_t heat[NUM_LEDS];

  float position = 9.0f;
  float fractional = 0.0f;

  uint32_t seed = 0;
};

/* ================================================================
 * FIREBALL APP
 * ================================================================ */

class AppFireball {

public:

  void enter() {

    score = 0;
    lives = 3;

    speed = 160;

    position = 9.0f;

    state = RUN;

    last = millis();

    fire.reset();
  }

  void update(Gesture g) {

    if (state == DEAD) {

      if (g == G_TAP) {
        enter();
      }

      return;
    }

    if (state == BETWEEN) {

      if (g == G_TAP) {

        state = RUN;

        position = 9.0f;

        last = millis();

        fire.reset();
      }

      return;
    }

    if (state == RUN &&
        g == G_TAP) {

      /*
       * Successful defuse near LED 0.
       */
      if (position <= 1.8f) {

        score++;

        state =
          BETWEEN;

        speed =
          speed * 82 / 100;

        if (speed < 38) {
          speed = 38;
        }

      } else {

        lives--;

        if (lives == 0) {

          state = DEAD;

        } else {

          state = BETWEEN;
        }
      }
    }
  }

  void render() {

    clearLeds();

    uint32_t now =
      millis();

    if (state == RUN) {

      /*
       * Safe zone.
       */
      leds[0] =
        CRGB(
          0,
          110,
          25
        );

      /*
       * Move fireball.
       */
      float step =
        (float)(now - last) /
        (float)speed;

      last = now;

      position -= step;

      /*
       * Update diffused fire field.
       */
      fire.update(position);

      /*
       * Draw it.
       */
      fire.render();

      /*
       * Restore green safe-zone indication.
       * Fire may overlap it, so blend rather than hard replace.
       */
      if (position > 0.8f) {

        leds[0] =
          blendColor(
            CRGB(
              0,
              110,
              25
            ),
            leds[0],
            90
          );
      }

      /*
       * Fireball escaped off the end.
       */
      if (position < -0.8f) {

        lives--;

        if (lives == 0) {

          state = DEAD;

        } else {

          state = BETWEEN;
        }

        return;
      }

      /*
       * Bright head sparkle.
       */
      if (position >= 0.0f &&
          position < NUM_LEDS) {

        uint8_t p =
          (uint8_t)position;

        leds[p] =
          blendColor(
            leds[p],
            CRGB::White,
            160
          );
      }
    }

    else if (state == BETWEEN) {

      /*
       * Score shown in binary.
       */
      for (uint8_t i = 0; i < NUM_LEDS; i++) {

        if (score & (1 << i)) {

          leds[i] =
            CRGB::White;

        } else {

          leds[i] =
            CRGB::Black;
        }
      }

      /*
       * Flash score state subtly.
       */
      if ((millis() / 180) & 1) {

        for (uint8_t i = 0; i < NUM_LEDS; i++) {

          if (score & (1 << i)) {

            leds[i] =
              blendColor(
                leds[i],
                CRGB(
                  255,
                  180,
                  70
                ),
                70
              );
          }
        }
      }
    }

    else {

      /*
       * DEAD.
       */
      uint8_t pulse =
        beatsin8(
          4,
          120,
          255
        );

      fill_solid(
        leds,
        NUM_LEDS,
        CRGB(
          pulse,
          0,
          0
        )
      );
    }
  }

private:

  enum {
    RUN,
    BETWEEN,
    DEAD
  };

  uint8_t state = RUN;

  uint8_t score = 0;
  uint8_t lives = 3;

  float position = 9.0f;

  uint16_t speed = 160;

  uint32_t last = 0;

  FireballFireEngine fire;
};

AppFireball appFireball;

/* ================================================================
 * MODE INSTANCES
 * ================================================================ */

ModeStatus   modeStatus;
ModeFire     modeFire;
ModeSaber    modeSaber;
ModeRainbow  modeRainbow;
ModePulse    modePulse;
ModeEntropy  modeEntropy;
ModeBinary   modeBinary;
ModeAmbient  modeAmbient;

Mode* mainModes[] = {
  &modeStatus,
  &modeFire,
  &modeSaber,
  &modeRainbow,
  &modePulse,
  &modeEntropy,
  &modeBinary,
  &modeAmbient
};

const uint8_t MAIN_MODE_COUNT =
  sizeof(mainModes) /
  sizeof(mainModes[0]);

/* ================================================================
 * UI VARIABLES
 * ================================================================ */

UIState uiState = UI_MODE;

uint8_t mainCursor = 0;
uint8_t appsCursor = 0;
uint8_t settingsCursor = 0;

uint8_t currentMainMode = 0;

uint32_t menuLastActivity = 0;

bool menuInit = false;

/* ================================================================
 * MENU HELPERS
 * ================================================================ */

void touchMenu() {
  menuLastActivity = millis();
}

bool inAnyMenu() {

  return
    uiState == UI_MENU_MAIN ||
    uiState == UI_MENU_APPS ||
    uiState == UI_MENU_SETTINGS ||
    uiState == UI_BRIGHTNESS ||
    uiState == UI_SPEED ||
    uiState == UI_INFO;
}

/* ================================================================
 * MAIN MENU
 * ================================================================ */

void renderMainMenu() {

  for (uint8_t i = 0; i < NUM_LEDS; i++) {

    leds[i] =
      CRGB(
        0,
        18,
        4
      );
  }

  /*
   * Apps portal.
   */
  leds[8] =
    CRGB(
      255,
      90,
      0
    );

  /*
   * Settings portal.
   */
  leds[9] =
    CRGB(
      0,
      50,
      100
    );

  CRGB pink =
    CRGB(
      255,
      40,
      160
    );

  pink.nscale8(
    beatsin8(
      7,
      210,
      255
    )
  );

  if (mainCursor < 10) {
    leds[mainCursor] = pink;
  }
}

/* ================================================================
 * APPS MENU
 * ================================================================ */

void renderAppsMenu() {

  clearLeds();

  /*
   * Back.
   */
  leds[0] =
    CRGB(
      255,
      100,
      0
    );

  /*
   * Fireball.
   */
  leds[1] =
    CRGB(
      255,
      0,
      0
    );

  CRGB cursor =
    CRGB(
      255,
      255,
      220
    );

  cursor.nscale8(
    beatsin8(
      9,
      200,
      255
    )
  );

  leds[appsCursor] =
    cursor;
}

/* ================================================================
 * SETTINGS MENU
 *
 * 0 = BACK
 * 1 = BRIGHTNESS
 * 2 = SPEED
 * 3 = INFO
 * 4 = SHUTDOWN
 * ================================================================ */

void renderSettingsMenu() {

  clearLeds();

  /*
   * Back.
   */
  leds[0] =
    CRGB(
      0,
      55,
      100
    );

  /*
   * Settings indicators.
   */
  for (uint8_t i = 1; i <= 4; i++) {

    leds[i] =
      CRGB(
        0,
        25,
        6
      );
  }

  /*
   * Shutdown indicator.
   */
  leds[4] =
    CRGB(
      75,
      0,
      0
    );

  CRGB pink =
    CRGB(
      255,
      40,
      160
    );

  if (settingsCursor == 0) {

    leds[0] =
      blendColor(
        leds[0],
        pink,
        200
      );

  } else {

    leds[settingsCursor] =
      pink;

    leds[settingsCursor].nscale8(
      beatsin8(
        7,
        200,
        255
      )
    );
  }
}

/* ================================================================
 * BRIGHTNESS
 * ================================================================ */

const uint8_t brightVals[10] = {
  18,
  24,
  32,
  42,
  55,
  70,
  85,
  100,
  115,
  130
};

uint8_t brightLevel = 3;

void renderBrightness() {

  for (uint8_t i = 0; i < NUM_LEDS; i++) {

    leds[i] =
      (
        i <= brightLevel
      )
      ? CRGB(
          255,
          50,
          160
        )
      : CRGB(
          0,
          20,
          5
        );
  }
}

/* ================================================================
 * SPEED
 * ================================================================ */

const uint8_t speedVals[10] = {
  40,
  55,
  70,
  85,
  100,
  120,
  145,
  170,
  200,
  240
};

uint8_t speedLevel = 4;

void renderSpeed() {

  for (uint8_t i = 0; i < NUM_LEDS; i++) {

    leds[i] =
      (
        i <= speedLevel
      )
      ? CRGB(
          255,
          50,
          160
        )
      : CRGB(
          0,
          20,
          5
        );
  }
}

/* ================================================================
 * INFO
 * ================================================================ */

uint8_t infoPage = 0;

void renderInfo() {

  clearLeds();

  if (infoPage == 0) {

    fill_solid(
      leds,
      NUM_LEDS,
      CRGB(
        0,
        16,
        20
      )
    );

    leds[0] =
      CRGB(
        0,
        110,
        255
      );
  }

  else if (infoPage == 1) {

    uint8_t lit =
      (millis() / 1000) %
      11;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      leds[i] =
        (
          i < lit
        )
        ? CRGB(
            0,
            35,
            12
          )
        : CRGB::Black;
    }
  }

  else {

    fill_solid(
      leds,
      NUM_LEDS,
      CRGB(
        25,
        6,
        0
      )
    );
  }
}

/* ================================================================
 * SOFTWARE SHUTDOWN
 *
 * Once entered:
 *   - LEDs are black
 *   - normal UI is inaccessible
 *   - wake requires a deliberate long hold
 *
 * We require the button to be released first so the hold used to
 * select Shutdown cannot immediately wake the system.
 * ================================================================ */

bool shutdownWaitingForRelease = true;

uint32_t shutdownPressStart = 0;

void enterShutdown() {

  uiState =
    UI_SLEEP;

  shutdownWaitingForRelease = true;

  shutdownPressStart = 0;

  clearLeds();

  FastLED.setBrightness(0);

  FastLED.show();
}

bool updateShutdown() {

  /*
   * Always keep outputs physically dark.
   */
  clearLeds();

  FastLED.setBrightness(0);

  FastLED.show();

  /*
   * First require the button to be released.
   */
  if (shutdownWaitingForRelease) {

    if (digitalRead(BUTTON_PIN) == HIGH) {

      shutdownWaitingForRelease = false;
    }

    return false;
  }

  /*
   * Now look for a deliberate long press.
   */
  bool pressed =
    digitalRead(BUTTON_PIN) == LOW;

  if (pressed) {

    if (shutdownPressStart == 0) {

      shutdownPressStart =
        millis();
    }

    /*
     * 1.5 second wake hold.
     */
    if (millis() - shutdownPressStart >= 1500) {

      shutdownPressStart = 0;

      applyBrightness();

      uiState =
        UI_MODE;

      currentMainMode = 0;

      mainModes[0]->enter();

      /*
       * Wait for button release after wake.
       */
      shutdownWaitingForRelease = true;

      return true;
    }

  } else {

    shutdownPressStart = 0;
  }

  return false;
}

/* ================================================================
 * UI HANDLER
 * ================================================================ */

void handleUI(Gesture g) {

  if (inAnyMenu()) {
    touchMenu();
  }

  /*
   * Very-long-hold always returns home EXCEPT while asleep.
   */
  if (g == G_VERY_LONG) {

    fill_solid(
      leds,
      NUM_LEDS,
      CRGB::Red
    );

    FastLED.show();

    delay(260);

    uiState =
      UI_MODE;

    currentMainMode = 0;

    mainModes[0]->enter();

    return;
  }

  /* ============================================================
   * FIREBALL APP
   * ============================================================ */

  if (uiState == UI_IN_APP) {

    if (g == G_LONG_HOLD) {

      uiState =
        UI_MENU_APPS;

      touchMenu();

      return;
    }

    appFireball.update(g);

    return;
  }

  /* ============================================================
   * NORMAL MODE
   * ============================================================ */

  if (uiState == UI_MODE) {

    if (g == G_HOLD) {

      if (!menuInit) {

        mainCursor =
          currentMainMode;

        menuInit = true;
      }

      uiState =
        UI_MENU_MAIN;

      touchMenu();

      return;
    }

    if (g == G_LONG_HOLD) {

      currentMainMode = 0;

      mainModes[0]->enter();

      return;
    }

    mainModes[currentMainMode]->update(g);

    return;
  }

  /* ============================================================
   * MAIN MENU
   * ============================================================ */

  if (uiState == UI_MENU_MAIN) {

    if (g == G_TAP) {

      mainCursor =
        (mainCursor + 1) %
        10;

      return;
    }

    if (g == G_HOLD) {

      if (mainCursor == 8) {

        appsCursor = 0;

        uiState =
          UI_MENU_APPS;

        touchMenu();
      }

      else if (mainCursor == 9) {

        settingsCursor = 0;

        uiState =
          UI_MENU_SETTINGS;

        touchMenu();
      }

      else {

        if (mainCursor < MAIN_MODE_COUNT) {

          currentMainMode =
            mainCursor;

          mainModes[currentMainMode]->enter();

          uiState =
            UI_MODE;
        }
      }

      return;
    }

    if (g == G_LONG_HOLD) {

      uiState =
        UI_MODE;

      return;
    }

    return;
  }

  /* ============================================================
   * APPS
   * ============================================================ */

  if (uiState == UI_MENU_APPS) {

    if (g == G_TAP) {

      appsCursor =
        (appsCursor + 1) % 2;

      return;
    }

    if (g == G_HOLD) {

      if (appsCursor == 0) {

        uiState =
          UI_MENU_MAIN;

        touchMenu();
      }

      else {

        appFireball.enter();

        uiState =
          UI_IN_APP;

        touchMenu();
      }

      return;
    }

    if (g == G_LONG_HOLD) {

      uiState =
        UI_MENU_MAIN;

      touchMenu();

      return;
    }

    return;
  }

  /* ============================================================
   * SETTINGS
   * ============================================================ */

  if (uiState == UI_MENU_SETTINGS) {

    if (g == G_TAP) {

      settingsCursor =
        (settingsCursor + 1) %
        5;

      return;
    }

    if (g == G_HOLD) {

      switch (settingsCursor) {

        case 0:

          uiState =
            UI_MENU_MAIN;

          touchMenu();

          break;

        case 1: {

          uint8_t best = 255;

          brightLevel = 0;

          for (uint8_t i = 0; i < 10; i++) {

            uint8_t d =
              abs(
                (int)brightVals[i] -
                (int)globalBrightness
              );

            if (d < best) {

              best = d;

              brightLevel = i;
            }
          }

          uiState =
            UI_BRIGHTNESS;

          touchMenu();

          break;
        }

        case 2:

          uiState =
            UI_SPEED;

          touchMenu();

          break;

        case 3:

          infoPage = 0;

          uiState =
            UI_INFO;

          touchMenu();

          break;

        case 4:

          /*
           * SHUTDOWN
           */
          enterShutdown();

          break;
      }

      return;
    }

    if (g == G_LONG_HOLD) {

      uiState =
        UI_MODE;

      currentMainMode = 0;

      mainModes[0]->enter();

      return;
    }

    return;
  }

  /* ============================================================
   * BRIGHTNESS
   * ============================================================ */

  if (uiState == UI_BRIGHTNESS) {

    if (g == G_TAP) {

      brightLevel =
        (brightLevel + 1) %
        10;

      globalBrightness =
        brightVals[brightLevel];

      applyBrightness();

      return;
    }

    if (g == G_HOLD ||
        g == G_LONG_HOLD) {

      uiState =
        UI_MENU_SETTINGS;

      touchMenu();

      return;
    }

    return;
  }

  /* ============================================================
   * SPEED
   * ============================================================ */

  if (uiState == UI_SPEED) {

    if (g == G_TAP) {

      speedLevel =
        (speedLevel + 1) %
        10;

      globalSpeed =
        speedVals[speedLevel];

      return;
    }

    if (g == G_HOLD ||
        g == G_LONG_HOLD) {

      uiState =
        UI_MENU_SETTINGS;

      touchMenu();

      return;
    }

    return;
  }

  /* ============================================================
   * INFO
   * ============================================================ */

  if (uiState == UI_INFO) {

    if (g == G_TAP) {

      infoPage =
        (infoPage + 1) %
        3;

      return;
    }

    if (g == G_HOLD ||
        g == G_LONG_HOLD) {

      uiState =
        UI_MENU_SETTINGS;

      touchMenu();

      return;
    }

    return;
  }
}

/* ================================================================
 * RENDER UI
 * ================================================================ */

void renderUI() {

  switch (uiState) {

    case UI_MODE:

      mainModes[currentMainMode]->render();

      break;

    case UI_MENU_MAIN:

      renderMainMenu();

      break;

    case UI_MENU_APPS:

      renderAppsMenu();

      break;

    case UI_MENU_SETTINGS:

      renderSettingsMenu();

      break;

    case UI_BRIGHTNESS:

      renderBrightness();

      break;

    case UI_SPEED:

      renderSpeed();

      break;

    case UI_INFO:

      renderInfo();

      break;

    case UI_IN_APP:

      appFireball.render();

      break;

    case UI_SLEEP:

      clearLeds();

      break;

    default:

      clearLeds();

      break;
  }
}

/* ================================================================
 * STARTUP
 * ================================================================ */

void startupAnimation() {

  applyBrightness();

  /*
   * Portal color sweep.
   */
  for (uint8_t i = 0; i < NUM_LEDS; i++) {

    leds[i] =
      CHSV(
        i * 25,
        230,
        210
      );

    FastLED.show();

    delay(18);
  }

  delay(70);

  clearLeds();

  FastLED.show();
}

/* ================================================================
 * SETUP
 * ================================================================ */

void setup() {

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(
    leds,
    NUM_LEDS
  );

  FastLED.setDither(0);

  applyBrightness();

  FastLED.clear(true);

  entropy.begin();

  button.begin();

  startupAnimation();

  currentMainMode = 0;

  mainModes[0]->enter();

  uiState =
    UI_MODE;
}

/* ================================================================
 * MAIN LOOP
 * ================================================================ */

void loop() {

  static uint32_t lastFrame = 0;

  uint32_t now =
    millis();

  /*
   * ============================================================
   * SOFTWARE SLEEP
   * ============================================================
   */

  if (uiState == UI_SLEEP) {

    updateShutdown();

    /*
     * Do not run normal UI, entropy, or animation logic while
     * sleeping.
     */
    delay(5);

    return;
  }

  /*
   * ============================================================
   * NORMAL SYSTEM
   * ============================================================
   */

  entropy.update();

  /*
   * Automatic menu timeout.
   */
  if (inAnyMenu() &&
      now - menuLastActivity >
      MENU_TIMEOUT_MS) {

    uiState =
      UI_MODE;

    currentMainMode = 0;

    mainModes[0]->enter();
  }

  /*
   * Button processing.
   */
  Gesture g =
    button.poll(
      inAnyMenu() ||
      uiState == UI_IN_APP
    );

  if (g != G_NONE) {

    handleUI(g);
  }

  /*
   * Rendering.
   */
  if (now - lastFrame >= FRAME_MS) {

    lastFrame = now;

    renderUI();

    FastLED.show();
  }
}