/*
 * ================================================================
 * LAMOKA1 DEV BRAIN — Portal Interface Engine V1.3
 * ================================================================
 *
 * FIXED:
 *   - Removed nonexistent entropy.next16()
 *   - Uses entropy.next() for 16-bit random values
 *   - Fire remains slow, organic, persistent, and laterally moving
 *   - Lightsaber randomized launch behavior preserved
 *   - Fireball retains diffused fireball + binary score display
 *   - Shutdown indicator is BLUE
 *
 * CONTROLS:
 *
 * NORMAL MODE:
 *   Tap          = mode action
 *   Double tap  = secondary action
 *   Hold        = main menu
 *   Long hold   = return to status
 *   Very long   = force home
 *
 * MAIN MENU:
 *   Tap          = next item
 *   Hold        = select
 *   Long hold   = return
 *
 * SETTINGS:
 *   0 = Back
 *   1 = Brightness
 *   2 = Speed
 *   3 = Info
 *   4 = Shutdown
 *
 * SHUTDOWN:
 *   Release button, then hold for 1.5 seconds to wake.
 *
 * ================================================================
 */

#include <Arduino.h>
#include <FastLED.h>

/* ================================================================
 * HARDWARE
 * ================================================================ */

#define LED_PIN             11
#define NUM_LEDS            10
#define BUTTON_PIN          0

#define COLOR_ENTROPY_PIN   5

#define ADC0                26
#define ADC1                27
#define ADC2                28
#define ADC3                29

#define DEFAULT_BRIGHTNESS  36
#define DEFAULT_SPEED       100

#define FRAME_MS            16
#define MENU_TIMEOUT_MS     16000UL

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

/* ================================================================
 * LED HELPERS
 * ================================================================ */

void applyBrightness() {
  uint8_t b = globalBrightness;

  if (b < 18) {
    b = 18;
  }

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
    return (uint8_t)(next() & 0xFF);
  }

  uint16_t range(uint16_t maximum) {

    if (maximum == 0) {
      return 0;
    }

    return (uint16_t)(next() % (maximum + 1));
  }

  bool coin() {
    return (next() & 1U) != 0;
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

    /* ------------------------------------------------------------
     * PRESS
     * ------------------------------------------------------------ */

    if (pressed && !wasPressed) {

      wasPressed = true;
      pressStart = now;

      holdFired = false;
      longFired = false;
      veryLongFired = false;

      return G_NONE;
    }

    /* ------------------------------------------------------------
     * HOLD
     * ------------------------------------------------------------ */

    if (pressed) {

      uint32_t held =
        now - pressStart;

      if (held >= 2800 &&
          !veryLongFired) {

        veryLongFired = true;

        return G_VERY_LONG;
      }

      if (held >= 1200 &&
          !longFired) {

        longFired = true;

        return G_LONG_HOLD;
      }

      if (held >= 550 &&
          !holdFired) {

        holdFired = true;

        return G_HOLD;
      }
    }

    /* ------------------------------------------------------------
     * RELEASE
     * ------------------------------------------------------------ */

    if (!pressed && wasPressed) {

      wasPressed = false;

      uint32_t held =
        now - pressStart;

      if (!holdFired &&
          !longFired &&
          !veryLongFired &&
          held < 400) {

        if (inMenu) {

          return G_TAP;
        }

        if (lastTap &&
            now - lastTap < 380) {

          lastTap = 0;
          pending = false;

          return G_DOUBLE;
        }

        lastTap = now;
        pending = true;
      }
    }

    /* ------------------------------------------------------------
     * SINGLE TAP TIMEOUT
     * ------------------------------------------------------------ */

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
 * FIRE COLOR
 * ================================================================ */

CRGB flameColor(uint8_t heat) {

  if (heat < 30) {

    return blendColor(
      CRGB(8, 0, 0),
      CRGB(45, 2, 0),
      heat * 8
    );
  }

  if (heat < 75) {

    return blendColor(
      CRGB(45, 2, 0),
      CRGB(145, 10, 0),
      scale8(
        heat - 30,
        255
      )
    );
  }

  if (heat < 145) {

    return blendColor(
      CRGB(145, 10, 0),
      CRGB(255, 65, 0),
      scale8(
        heat - 75,
        255
      )
    );
  }

  if (heat < 220) {

    return blendColor(
      CRGB(255, 65, 0),
      CRGB(255, 165, 25),
      scale8(
        heat - 145,
        255
      )
    );
  }

  return blendColor(
    CRGB(255, 165, 25),
    CRGB(255, 240, 145),
    scale8(
      heat - 220,
      255
    )
  );
}

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
      28 +
      80 * 100 /
      (globalSpeed + 50);

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

      int p =
        pos - dir * t;

      if (p >= 0 &&
          p < NUM_LEDS) {

        leds[p] =
          CRGB(
            0,
            25 + t * 35,
            70 + t * 55
          );
      }
    }

    leds[pos] =
      CRGB(
        0,
        70,
        255
      );
  }

private:

  int8_t pos = 0;
  int8_t dir = 1;

  uint32_t last = 0;
};

/* ================================================================
 * FIREPLACE
 *
 * This version intentionally prioritizes:
 *
 *   - persistence
 *   - smoothness
 *   - low-frequency movement
 *   - sideways drift
 *   - upward convection
 *   - random flame tongues
 *   - warm base/embers
 *
 * It should not look like ten independent LEDs turning on/off.
 * ================================================================ */

class ModeFire : public Mode {
public:

  void enter() override {

    memset(
      heat,
      0,
      sizeof(heat)
    );

    windPhase =
      (uint16_t)(entropy.next() & 0xFFFF);

    windSpeed =
      280 +
      entropy.range(420);

    flare = 0;
    bigFlare = 0;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      heat[i] =
        25 +
        entropy.range(45);
    }
  }

  void update(Gesture g) override {

    if (g == G_TAP) {

      flare = 170;
    }

    if (g == G_DOUBLE) {

      bigFlare = 230;
    }
  }

  void render() override {

    uint32_t now = millis();

    /* ------------------------------------------------------------
     * LARGE-SCALE WIND
     * ------------------------------------------------------------ */

    uint8_t windNoise =
      inoise8(
        (uint16_t)(now / windSpeed) +
        windPhase
      );

    int wind =
      ((int)windNoise - 128) / 2;

    /* ------------------------------------------------------------
     * MEDIUM-SCALE TURBULENCE
     * ------------------------------------------------------------ */

    uint8_t turbulence =
      inoise8(
        (uint16_t)(now / 43),
        191
      );

    /* ------------------------------------------------------------
     * GENTLE COOLING
     *
     * This is deliberately slow.
     * ------------------------------------------------------------ */

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t cooling =
        1 +
        entropy.range(3);

      /*
       * Upper flame loses a little more heat.
       */
      cooling +=
        i / 5;

      if (cooling > heat[i]) {

        heat[i] = 0;

      } else {

        heat[i] -= cooling;
      }
    }

    /* ------------------------------------------------------------
     * SIDEWAYS FLAME MOVEMENT
     * ------------------------------------------------------------ */

    uint8_t sidewaysAmount =
      20 +
      (turbulence >> 5);

    uint8_t displaced[NUM_LEDS];

    memcpy(
      displaced,
      heat,
      sizeof(heat)
    );

    for (uint8_t i = 1;
         i < NUM_LEDS - 1;
         i++) {

      int source =
        i;

      if (wind > 14) {

        source = i - 1;

      } else if (wind < -14) {

        source = i + 1;
      }

      if (source < 0) {
        source = 0;
      }

      if (source >= NUM_LEDS) {
        source = NUM_LEDS - 1;
      }

      displaced[i] =
        lerp8by8(
          heat[i],
          heat[source],
          sidewaysAmount
        );
    }

    memcpy(
      heat,
      displaced,
      sizeof(heat)
    );

    /* ------------------------------------------------------------
     * UPWARD CONVECTION
     * ------------------------------------------------------------ */

    for (int i = NUM_LEDS - 1;
         i >= 2;
         i--) {

      uint16_t average =
        (uint16_t)heat[i - 1] +
        (uint16_t)heat[i - 2];

      average /= 2;

      /*
       * Preserve most of the existing heat.
       */
      heat[i] =
        lerp8by8(
          heat[i],
          (uint8_t)average,
          175
        );
    }

    /* ------------------------------------------------------------
     * FIREBASE / EMBERS
     * ------------------------------------------------------------ */

    uint8_t baseFuel =
      100 +
      entropy.range(55);

    heat[0] =
      qadd8(
        heat[0],
        baseFuel
      );

    heat[1] =
      qadd8(
        heat[1],
        30 +
        entropy.range(35)
      );

    /* ------------------------------------------------------------
     * RANDOM FLAME LICKS
     * ------------------------------------------------------------ */

    /*
     * Only occasionally create one.
     * This makes the randomness feel organic instead of noisy.
     */

    if (entropy.next8() > 220) {

      uint8_t tongue =
        2 +
        entropy.range(5);

      uint8_t strength =
        60 +
        entropy.range(100);

      heat[tongue] =
        qadd8(
          heat[tongue],
          strength
        );

      /*
       * Natural leaning.
       */
      if (entropy.coin() &&
          tongue + 1 < NUM_LEDS) {

        heat[tongue + 1] =
          qadd8(
            heat[tongue + 1],
            strength / 2
          );
      }

      if (!entropy.coin() &&
          tongue > 2) {

        heat[tongue - 1] =
          qadd8(
            heat[tongue - 1],
            strength / 3
          );
      }
    }

    /* ------------------------------------------------------------
     * SECONDARY SMALL FLAME LICK
     * ------------------------------------------------------------ */

    if (entropy.next8() > 238) {

      uint8_t lick =
        3 +
        entropy.range(4);

      heat[lick] =
        qadd8(
          heat[lick],
          35 +
          entropy.range(70)
        );
    }

    /* ------------------------------------------------------------
     * USER FLARE
     * ------------------------------------------------------------ */

    if (flare) {

      heat[0] =
        qadd8(
          heat[0],
          flare
        );

      heat[1] =
        qadd8(
          heat[1],
          flare / 2
        );

      if (flare > 8) {

        flare -= 8;

      } else {

        flare = 0;
      }
    }

    /* ------------------------------------------------------------
     * BIG FLARE
     * ------------------------------------------------------------ */

    if (bigFlare) {

      for (uint8_t i = 0; i < 5; i++) {

        heat[i] =
          qadd8(
            heat[i],
            bigFlare
          );
      }

      if (bigFlare > 10) {

        bigFlare -= 10;

      } else {

        bigFlare = 0;
      }
    }

    /* ------------------------------------------------------------
     * FINAL LOW-FREQUENCY FLICKER
     *
     * Never allowed to collapse the flame to zero.
     * ------------------------------------------------------------ */

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t detail =
        inoise8(
          (uint16_t)(now / 31) +
          i * 37
        );

      uint8_t modulation =
        224 +
        (detail >> 5);

      heat[i] =
        scale8(
          heat[i],
          modulation
        );
    }

    /* ------------------------------------------------------------
     * DRAW
     * ------------------------------------------------------------ */

    for (uint8_t i = 0; i < NUM_LEDS; i++) {

      uint8_t h =
        heat[i];

      /*
       * The very bottom never completely dies.
       */
      if (i == 0 &&
          h < 58) {

        h = 58;
      }

      CRGB c =
        flameColor(h);

      /*
       * Tiny high-heat sparkle.
       */
      if (h > 175 &&
          entropy.next8() > 240) {

        c =
          blendColor(
            c,
            CRGB(
              255,
              220,
              110
            ),
            65
          );
      }

      leds[i] = c;
    }
  }

private:

  uint8_t heat[NUM_LEDS];

  uint16_t windPhase = 0;
  uint16_t windSpeed = 400;

  uint8_t flare = 0;
  uint8_t bigFlare = 0;
};

/* ================================================================
 * LIGHTSABER
 * ================================================================ */

class ModeSaber : public Mode {
public:

  void enter() override {

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

    power = 0;

    extending = true;

    ignitionTime =
      millis();

    clash = 0;
  }

  void update(Gesture g) override {

    if (g == G_TAP) {

      if (!extending) {

        launchRandom();

      } else {

        extending = false;
      }
    }

    if (g == G_DOUBLE) {

      launchRandom();
    }

    if (g == G_HOLD) {

      clash = 16;
    }
  }

  void render() override {

    clearLeds();

    uint32_t now =
      millis();

    if (extending) {

      uint8_t launchRate =
        7 +
        globalSpeed / 30;

      if (pattern == 1) {
        launchRate += 4;
      }

      if (pattern == 3 &&
          launchRate > 2) {
        launchRate -= 1;
      }

      if (launchRate < 2) {
        launchRate = 2;
      }

      power =
        qadd8(
          power,
          launchRate
        );

    } else {

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

    for (uint8_t i = 0;
         i < visibleLength;
         i++) {

      uint8_t brightness =
        qsub8(
          255,
          i * 11
        );

      /* Unstable */
      if (pattern == 3) {

        if (entropy.next8() < instability) {

          brightness =
            scale8(
              brightness,
              120
            );
        }
      }

      /* Crackling */
      if (pattern == 5) {

        if (entropy.next8() > 220) {

          brightness = 255;
        }
      }

      /* Scanning */
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

      /* Explosive */
      if (pattern == 1 &&
          i == 0) {

        brightness = 255;
      }

      /* Pulse */
      if (pattern == 2) {

        uint8_t pulse =
          beatsin8(
            9,
            185,
            255
          );

        brightness =
          scale8(
            brightness,
            pulse
          );
      }

      leds[i] =
        CHSV(
          hue,
          saturation,
          brightness
        );

      uint8_t core =
        (pattern == 1)
        ? 95
        : 75;

      if (i < 2) {

        leds[i] =
          blendColor(
            leds[i],
            CRGB::White,
            core
          );
      }
    }

    /* Launch flash */

    if (extending &&
        now - ignitionTime < 170) {

      if (pattern == 1) {

        for (uint8_t i = 0;
             i < NUM_LEDS;
             i++) {

          if (entropy.next8() > 120) {

            leds[i] =
              blendColor(
                leds[i],
                CRGB::White,
                120
              );
          }
        }

      } else if (visibleLength > 0) {

        leds[0] =
          blendColor(
            leds[0],
            CRGB::White,
            180
          );
      }
    }

    /* Clash */

    if (clash) {

      for (uint8_t i = 0;
           i < visibleLength;
           i++) {

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

    /* Sparkle */

    if (visibleLength > 0 &&
        entropy.next8() > 220) {

      uint8_t p =
        entropy.next8() %
        visibleLength;

      leds[p] =
        blendColor(
          leds[p],
          CRGB::White,
          150
        );
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

  uint32_t ignitionTime = 0;

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

    power = 0;

    extending = true;

    ignitionTime =
      millis();
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

      reverse =
        !reverse;
    }
  }

  void render() override {

    uint8_t step =
      speedMode *
      (globalSpeed + 40) /
      75;

    if (!step) {
      step = 1;
    }

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

    if (r < 2) {
      r = 2;
    }

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

    for (uint8_t i = 0;
         i < NUM_LEDS;
         i++) {

      uint8_t n =
        inoise8(
          timeValue +
          i * 42
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
 * BINARY MODE
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

    for (uint8_t i = 0;
         i < NUM_LEDS;
         i++) {

      leds[i] =
        (value & (1 << i))
        ? CRGB(0, 180, 255)
        : CRGB(8, 6, 14);
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

    for (uint8_t i = 0;
         i < NUM_LEDS;
         i++) {

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
 * FIREBALL FIRE ENGINE
 * ================================================================ */

class FireballFireEngine {
public:

  void reset() {

    memset(
      heat,
      0,
      sizeof(heat)
    );

    position =
      NUM_LEDS - 1;

    seed =
      entropy.next();
  }

  void update(float newPosition) {

    position =
      newPosition;

    /* ------------------------------------------------------------
     * COOL
     * ------------------------------------------------------------ */

    for (uint8_t i = 0;
         i < NUM_LEDS;
         i++) {

      uint8_t cooling =
        6 +
        entropy.range(6);

      if (cooling > heat[i]) {

        heat[i] = 0;

      } else {

        heat[i] -= cooling;
      }
    }

    /* ------------------------------------------------------------
     * DIFFUSE
     * ------------------------------------------------------------ */

    for (int i = NUM_LEDS - 1;
         i >= 2;
         i--) {

      uint16_t average =
        (uint16_t)heat[i - 1] +
        (uint16_t)heat[i - 2];

      average /= 2;

      heat[i] =
        lerp8by8(
          heat[i],
          (uint8_t)average,
          175
        );
    }

    int head =
      (int)(position + 0.5f);

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
            145
          );
      }

      if (head - 1 >= 0) {

        heat[head - 1] =
          qadd8(
            heat[head - 1],
            95
          );
      }
    }

    /* ------------------------------------------------------------
     * LOCAL GLOW
     * ------------------------------------------------------------ */

    for (uint8_t i = 1;
         i < NUM_LEDS - 1;
         i++) {

      uint16_t local =
        (uint16_t)heat[i - 1] +
        (uint16_t)heat[i] +
        (uint16_t)heat[i + 1];

      local /= 3;

      heat[i] =
        qadd8(
          heat[i],
          scale8(
            (uint8_t)local,
            55
          )
        );
    }

    seed +=
      27 +
      entropy.range(50);
  }

  void render() {

    for (uint8_t i = 0;
         i < NUM_LEDS;
         i++) {

      uint8_t h =
        heat[i];

      if (h == 0) {

        leds[i] =
          CRGB::Black;

        continue;
      }

      CRGB c =
        flameColor(h);

      uint8_t noise =
        inoise8(
          seed +
          i * 37
        );

      if (noise < 60) {

        c.nscale8(190);
      }

      leds[i] = c;
    }

    int head =
      (int)(position + 0.5f);

    if (head >= 0 &&
        head < NUM_LEDS) {

      leds[head] =
        blendColor(
          leds[head],
          CRGB(
            255,
            225,
            85
          ),
          230
        );
    }
  }

private:

  uint8_t heat[NUM_LEDS];

  float position = 9.0f;

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

    /* ------------------------------------------------------------
     * RUN
     * ------------------------------------------------------------ */

    if (state == RUN) {

      /*
       * SAFE ZONE
       */
      leds[0] =
        CRGB(
          0,
          110,
          25
        );

      float step =
        (float)(now - last) /
        (float)speed;

      last = now;

      position -= step;

      fire.update(position);
      fire.render();

      /*
       * Re-establish the safe-zone glow.
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
            100
          );
      }

      /*
       * Fireball passed the safe zone.
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
       * Hot core.
       */
      if (position >= 0.0f &&
          position < NUM_LEDS) {

        uint8_t p =
          (uint8_t)position;

        leds[p] =
          blendColor(
            leds[p],
            CRGB::White,
            155
          );
      }

      return;
    }

    /* ------------------------------------------------------------
     * BETWEEN LAUNCHES
     *
     * ORIGINAL SIMPLE BINARY SCORE
     *
     * score bit 0 = LED 0
     * score bit 1 = LED 1
     * ...
     * score bit 9 = LED 9
     * ------------------------------------------------------------ */

    if (state == BETWEEN) {

      for (uint8_t i = 0;
           i < NUM_LEDS;
           i++) {

        if (score & (1 << i)) {

          leds[i] =
            CRGB::White;

        } else {

          leds[i] =
            CRGB::Black;
        }
      }

      return;
    }

    /* ------------------------------------------------------------
     * DEAD
     * ------------------------------------------------------------ */

    if (state == DEAD) {

      uint8_t pulse =
        beatsin8(
          4,
          125,
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

ModeStatus modeStatus;
ModeFire modeFire;
ModeSaber modeSaber;
ModeRainbow modeRainbow;
ModePulse modePulse;
ModeEntropy modeEntropy;
ModeBinary modeBinary;
ModeAmbient modeAmbient;

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

  for (uint8_t i = 0;
       i < NUM_LEDS;
       i++) {

    leds[i] =
      CRGB(
        0,
        18,
        4
      );
  }

  leds[8] =
    CRGB(
      255,
      90,
      0
    );

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

  leds[mainCursor] = pink;
}

/* ================================================================
 * APPS MENU
 * ================================================================ */

void renderAppsMenu() {

  clearLeds();

  leds[0] =
    CRGB(
      255,
      100,
      0
    );

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
 * SETTINGS
 *
 * 0 = Back
 * 1 = Brightness
 * 2 = Speed
 * 3 = Info
 * 4 = Shutdown
 * ================================================================ */

void renderSettingsMenu() {

  clearLeds();

  leds[0] =
    CRGB(
      0,
      55,
      100
    );

  leds[1] =
    CRGB(
      0,
      25,
      6
    );

  leds[2] =
    CRGB(
      0,
      25,
      6
    );

  leds[3] =
    CRGB(
      0,
      25,
      6
    );

  /*
   * BLUE SHUTDOWN DOT
   */
  leds[4] =
    CRGB(
      0,
      70,
      255
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

  for (uint8_t i = 0;
       i < NUM_LEDS;
       i++) {

    leds[i] =
      (i <= brightLevel)
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

  for (uint8_t i = 0;
       i < NUM_LEDS;
       i++) {

    leds[i] =
      (i <= speedLevel)
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

  } else if (infoPage == 1) {

    uint8_t lit =
      (millis() / 1000) % 11;

    for (uint8_t i = 0;
         i < NUM_LEDS;
         i++) {

      leds[i] =
        (i < lit)
        ? CRGB(
            0,
            35,
            12
          )
        : CRGB::Black;
    }

  } else {

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
 * ================================================================ */

bool shutdownWaitingForRelease = true;
uint32_t shutdownPressStart = 0;

void enterShutdown() {

  uiState =
    UI_SLEEP;

  shutdownWaitingForRelease =
    true;

  shutdownPressStart = 0;

  clearLeds();

  FastLED.setBrightness(0);

  FastLED.show();
}

void updateShutdown() {

  clearLeds();

  FastLED.setBrightness(0);

  FastLED.show();

  /*
   * Button must be released after shutdown selection.
   */
  if (shutdownWaitingForRelease) {

    if (digitalRead(BUTTON_PIN) == HIGH) {

      shutdownWaitingForRelease = false;
    }

    return;
  }

  bool pressed =
    digitalRead(BUTTON_PIN) == LOW;

  if (pressed) {

    if (shutdownPressStart == 0) {

      shutdownPressStart = millis();
    }

    /*
     * 1.5 second wake hold.
     */
    if (millis() -
        shutdownPressStart >= 1500) {

      shutdownPressStart = 0;

      applyBrightness();

      uiState =
        UI_MODE;

      currentMainMode = 0;

      mainModes[0]->enter();

      /*
       * Require release before another wake.
       */
      shutdownWaitingForRelease = true;
    }

  } else {

    shutdownPressStart = 0;
  }
}

/* ================================================================
 * UI HANDLER
 * ================================================================ */

void handleUI(Gesture g) {

  if (inAnyMenu()) {

    touchMenu();
  }

  /* ------------------------------------------------------------
   * VERY LONG HOLD
   * ------------------------------------------------------------ */

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

  /* ------------------------------------------------------------
   * FIREBALL
   * ------------------------------------------------------------ */

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

  /* ------------------------------------------------------------
   * NORMAL MODE
   * ------------------------------------------------------------ */

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

  /* ------------------------------------------------------------
   * MAIN MENU
   * ------------------------------------------------------------ */

  if (uiState == UI_MENU_MAIN) {

    if (g == G_TAP) {

      mainCursor =
        (mainCursor + 1) % 10;

      return;
    }

    if (g == G_HOLD) {

      if (mainCursor == 8) {

        appsCursor = 0;

        uiState =
          UI_MENU_APPS;

        touchMenu();

      } else if (mainCursor == 9) {

        settingsCursor = 0;

        uiState =
          UI_MENU_SETTINGS;

        touchMenu();

      } else {

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

  /* ------------------------------------------------------------
   * APPS
   * ------------------------------------------------------------ */

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

      } else {

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

  /* ------------------------------------------------------------
   * SETTINGS
   * ------------------------------------------------------------ */

  if (uiState == UI_MENU_SETTINGS) {

    if (g == G_TAP) {

      settingsCursor =
        (settingsCursor + 1) % 5;

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

          for (uint8_t i = 0;
               i < 10;
               i++) {

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

  /* ------------------------------------------------------------
   * BRIGHTNESS
   * ------------------------------------------------------------ */

  if (uiState == UI_BRIGHTNESS) {

    if (g == G_TAP) {

      brightLevel =
        (brightLevel + 1) % 10;

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

  /* ------------------------------------------------------------
   * SPEED
   * ------------------------------------------------------------ */

  if (uiState == UI_SPEED) {

    if (g == G_TAP) {

      speedLevel =
        (speedLevel + 1) % 10;

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

  /* ------------------------------------------------------------
   * INFO
   * ------------------------------------------------------------ */

  if (uiState == UI_INFO) {

    if (g == G_TAP) {

      infoPage =
        (infoPage + 1) % 3;

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
 * STARTUP ANIMATION
 * ================================================================ */

void startupAnimation() {

  applyBrightness();

  for (uint8_t i = 0;
       i < NUM_LEDS;
       i++) {

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

  /* ------------------------------------------------------------
   * SOFTWARE SLEEP
   * ------------------------------------------------------------ */

  if (uiState == UI_SLEEP) {

    updateShutdown();

    delay(5);

    return;
  }

  /* ------------------------------------------------------------
   * ENTROPY
   * ------------------------------------------------------------ */

  entropy.update();

  /* ------------------------------------------------------------
   * MENU TIMEOUT
   * ------------------------------------------------------------ */

  if (inAnyMenu() &&
      now - menuLastActivity >
      MENU_TIMEOUT_MS) {

    uiState =
      UI_MODE;

    currentMainMode = 0;

    mainModes[0]->enter();
  }

  /* ------------------------------------------------------------
   * BUTTON
   * ------------------------------------------------------------ */

  Gesture g =
    button.poll(
      inAnyMenu() ||
      uiState == UI_IN_APP
    );

  if (g != G_NONE) {

    handleUI(g);
  }

  /* ------------------------------------------------------------
   * FRAME
   * ------------------------------------------------------------ */

  if (now - lastFrame >= FRAME_MS) {

    lastFrame = now;

    renderUI();

    FastLED.show();
  }
}