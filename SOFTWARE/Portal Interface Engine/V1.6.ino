#include <Arduino.h>
#include <FastLED.h>
#include "pico/bootrom.h"

/*
 * ================================================================
 * LAMOKA1 DEV BRAIN — Portal Interface Engine V1.6
 * ================================================================
 *
 * V1.5 BASELINE PRESERVED
 * V1.6 ONLY CHANGES THE REQUESTED UI/GAME DETAILS
 *
 * HOME PAGE
 * ----------
 * 1 = BLUE STATUS BALL      -> CACTUS GREEN
 * 2 = LIGHTSABER            -> CACTUS GREEN
 * 3 = RAINBOW               -> CACTUS GREEN
 * 4 = GAMES                 -> V1.5 ORANGE
 * 5 = SETTINGS              -> V1.5 BLUE
 *
 * CURSOR
 * ------
 * Every selected item is HOT PINK.
 *
 * DEFAULT IDLE
 * ------------
 * The original V1.5 blue bouncing status ball.
 *
 * GAMES
 * -----
 * FIREBALL
 * - No bright persistent trail
 * - Short local glow only
 * - Faster as score increases
 * - Hit/miss feedback
 *
 * SETTINGS
 * --------
 * BACK
 * BRIGHTNESS
 * SPEED
 * INFO
 * USB BOOTSEL
 * SHUT DOWN
 *
 * BOOTSEL
 * -------
 * reset_usb_boot(0, 0)
 *
 * IMPORTANT
 * ---------
 * No min() is used anywhere in this sketch.
 * This avoids the RP2040 Arduino/FastLED overload conflict.
 */


/* ================================================================
 * HARDWARE
 * ================================================================ */

#define LED_PIN             11
#define NUM_LEDS            10
#define BUTTON_PIN           0
#define COLOR_ENTROPY_PIN    5

#define ADC0                26
#define ADC1                27
#define ADC2                28
#define ADC3                29

#define DEFAULT_BRIGHTNESS  18
#define DEFAULT_SPEED      100

#define FRAME_MS            16
#define MENU_TIMEOUT_MS     16000UL

CRGB leds[NUM_LEDS];


/* ================================================================
 * COLORS
 * ================================================================ */

const CRGB COLOR_HOT_PINK = CRGB(255, 40, 160);

/* V1.5 home mode color */
const CRGB COLOR_CACTUS = CRGB(0, 90, 18);

/* V1.5 Games portal color */
const CRGB COLOR_GAMES = CRGB(255, 90, 0);

/* V1.5 Settings portal color */
const CRGB COLOR_SETTINGS = CRGB(0, 50, 100);


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
  UI_IN_GAME,
  UI_SLEEP
};


/* ================================================================
 * GLOBAL SETTINGS
 * ================================================================ */

uint8_t globalBrightness = DEFAULT_BRIGHTNESS;
uint8_t globalSpeed = DEFAULT_SPEED;


/* ================================================================
 * UI GLOBALS
 * ================================================================ */

UIState uiState = UI_MODE;

/*
 * HOME:
 * 0 = STATUS BALL
 * 1 = LIGHTSABER
 * 2 = RAINBOW
 * 3 = GAMES
 * 4 = SETTINGS
 */
uint8_t mainCursor = 0;

/*
 * GAMES:
 * 0 = BACK
 * 1 = FIREBALL
 */
uint8_t gamesCursor = 0;

/*
 * SETTINGS:
 * 0 = BACK
 * 1 = BRIGHTNESS
 * 2 = SPEED
 * 3 = INFO
 * 4 = USB BOOTSEL
 * 5 = SHUT DOWN
 */
uint8_t settingsCursor = 0;

/*
 * CURRENT NORMAL MODE:
 * 0 = STATUS BALL
 * 1 = LIGHTSABER
 * 2 = RAINBOW
 */
uint8_t currentMainMode = 0;

uint32_t menuLastActivity = 0;


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

  fill_solid(
    leds,
    NUM_LEDS,
    CRGB::Black
  );
}


CRGB blendColor(
  const CRGB &a,
  const CRGB &b,
  uint8_t t
) {

  return CRGB(
    lerp8by8(a.r, b.r, t),
    lerp8by8(a.g, b.g, t),
    lerp8by8(a.b, b.b, t)
  );
}


/*
 * Universal cursor renderer.
 *
 * Every cursor in PIE is hot pink.
 */
void renderCursor(
  uint8_t ledIndex,
  uint8_t minBrightness = 200
) {

  if (ledIndex >= NUM_LEDS) {
    return;
  }

  CRGB c = COLOR_HOT_PINK;

  c.nscale8(
    beatsin8(
      7,
      minBrightness,
      255
    )
  );

  leds[ledIndex] = c;
}


/* ================================================================
 * ENTROPY SERVICE
 * ================================================================ */

class EntropyService {

  uint32_t s = 1;

  void mix() {

    uint32_t a = analogRead(ADC0);
    uint32_t b = analogRead(ADC1);
    uint32_t c = analogRead(ADC2);
    uint32_t d = analogRead(ADC3);

    s ^= (a << 16);
    s ^= b;
    s ^= (c << 8);
    s ^= d;
    s ^= micros();

    s ^= (
      (uint32_t)digitalRead(
        COLOR_ENTROPY_PIN
      ) << 23
    );

    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;

    s += 0x9E3779B9UL;
  }

public:

  void begin() {

    pinMode(ADC0, INPUT);
    pinMode(ADC1, INPUT);
    pinMode(ADC2, INPUT);
    pinMode(ADC3, INPUT);
    pinMode(COLOR_ENTROPY_PIN, INPUT);

    s ^= micros();

    for (
      uint16_t i = 0;
      i < 300;
      i++
    ) {
      mix();
    }
  }


  void update() {
    mix();
  }


  uint32_t next() {

    mix();

    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;

    return s;
  }


  uint8_t next8() {
    return (uint8_t)(next() & 255UL);
  }


  uint16_t range(
    uint16_t maximum
  ) {

    if (maximum == 0) {
      return 0;
    }

    return (uint16_t)(
      next() %
      ((uint32_t)maximum + 1UL)
    );
  }


  bool coin() {
    return (next() & 1UL) != 0;
  }
};


EntropyService entropy;


/* ================================================================
 * BUTTON SERVICE
 * ================================================================ */

class ButtonService {

  bool lastRaw = HIGH;
  bool wasPressed = false;
  bool pendingTap = false;

  bool holdFired = false;
  bool longFired = false;
  bool veryLongFired = false;

  uint32_t pressStart = 0;
  uint32_t debounceStart = 0;
  uint32_t lastTap = 0;

public:

  void begin() {

    pinMode(
      BUTTON_PIN,
      INPUT_PULLUP
    );

    lastRaw =
      digitalRead(BUTTON_PIN);

    debounceStart =
      millis();
  }


  Gesture poll(
    bool inMenu
  ) {

    bool raw =
      digitalRead(BUTTON_PIN);

    uint32_t now =
      millis();


    if (raw != lastRaw) {

      debounceStart = now;
      lastRaw = raw;
    }


    if (
      now - debounceStart < 28
    ) {
      return G_NONE;
    }


    bool pressed = !raw;


    /* PRESS */

    if (
      pressed &&
      !wasPressed
    ) {

      wasPressed = true;

      pressStart = now;

      holdFired = false;
      longFired = false;
      veryLongFired = false;

      return G_NONE;
    }


    /* HOLDING */

    if (pressed) {

      uint32_t held =
        now - pressStart;


      if (
        held >= 2800 &&
        !veryLongFired
      ) {

        veryLongFired = true;

        return G_VERY_LONG;
      }


      if (
        held >= 1200 &&
        !longFired
      ) {

        longFired = true;

        return G_LONG_HOLD;
      }


      if (
        held >= 550 &&
        !holdFired
      ) {

        holdFired = true;

        return G_HOLD;
      }
    }


    /* RELEASE */

    if (
      !pressed &&
      wasPressed
    ) {

      wasPressed = false;

      uint32_t held =
        now - pressStart;


      if (
        !holdFired &&
        !longFired &&
        !veryLongFired &&
        held < 400
      ) {

        if (inMenu) {
          return G_TAP;
        }


        if (
          lastTap &&
          now - lastTap < 380
        ) {

          lastTap = 0;
          pendingTap = false;

          return G_DOUBLE;
        }


        lastTap = now;
        pendingTap = true;
      }
    }


    /* DELAYED SINGLE TAP */

    if (
      !inMenu &&
      pendingTap &&
      lastTap &&
      now - lastTap >= 380
    ) {

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

  virtual void update(
    Gesture
  ) {}

  virtual void render() {}
};


/* ================================================================
 * STATUS — ORIGINAL V1.5 BLUE BOUNCING BALL
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


    /*
     * THIS IS THE ORIGINAL V1.5 MOVEMENT.
     */
    uint16_t iv =
      (uint16_t)(
        28 +
        8000 /
        (globalSpeed + 50)
      );


    uint32_t now =
      millis();


    if (
      now - last >= iv
    ) {

      last = now;

      p += d;


      if (p >= 9) {

        p = 9;
        d = -1;
      }


      if (p <= 0) {

        p = 0;
        d = 1;
      }
    }


    /*
     * THIS IS THE ORIGINAL V1.5 THREE-PIXEL
     * BLUE TRAIL / BOUNCE EFFECT.
     */
    for (
      int t = 0;
      t < 3;
      t++
    ) {

      int q =
        p -
        d * t;


      if (
        q >= 0 &&
        q < 10
      ) {

        leds[q] =
          CRGB(
            0,
            25 + t * 35,
            70 + t * 55
          );
      }
    }


    /*
     * Bright blue ball.
     */
    leds[p] =
      CRGB(
        0,
        70,
        255
      );
  }
};


/* ================================================================
 * LIGHTSABER
 * ================================================================ */

class ModeSaber : public Mode {

  uint8_t hue = 0;
  uint8_t sat = 220;

  uint8_t pattern = 0;
  uint8_t instability = 30;

  uint8_t power = 0;
  uint8_t clash = 0;

  bool extending = true;

  uint32_t ignition = 0;


  void launch() {

    hue =
      entropy.next8();

    sat =
      (uint8_t)(
        175 +
        entropy.range(65)
      );

    pattern =
      (uint8_t)entropy.range(6);

    instability =
      (uint8_t)(
        20 +
        entropy.range(80)
      );

    power = 0;
    extending = true;

    ignition = millis();
  }

public:

  void enter() override {
    launch();
  }


  void update(
    Gesture g
  ) override {

    if (g == G_TAP) {

      if (extending) {
        extending = false;
      }

      else {
        launch();
      }
    }


    if (g == G_DOUBLE) {
      launch();
    }


    if (g == G_HOLD) {
      clash = 16;
    }
  }


  void render() override {

    clearLeds();


    uint8_t rate =
      (uint8_t)(
        7 +
        globalSpeed / 30
      );


    if (pattern == 1) {
      rate += 4;
    }


    if (
      pattern == 3 &&
      rate > 2
    ) {
      rate--;
    }


    if (extending) {

      power =
        qadd8(
          power,
          rate
        );
    }

    else {

      power =
        qsub8(
          power,
          10
        );
    }


    uint8_t len =
      scale8(
        power,
        10
      );


    for (
      uint8_t i = 0;
      i < len;
      i++
    ) {

      uint8_t b =
        qsub8(
          255,
          (uint8_t)(i * 11)
        );


      if (
        pattern == 3 &&
        entropy.next8() < instability
      ) {

        b =
          scale8(
            b,
            120
          );
      }


      if (
        pattern == 5 &&
        entropy.next8() > 220
      ) {

        b = 255;
      }


      if (pattern == 4) {

        b =
          scale8(
            b,
            beatsin8(
              12,
              145,
              255
            )
          );
      }


      if (pattern == 2) {

        b =
          scale8(
            b,
            beatsin8(
              9,
              185,
              255
            )
          );
      }


      if (
        pattern == 1 &&
        i == 0
      ) {

        b = 255;
      }


      leds[i] =
        CHSV(
          hue,
          sat,
          b
        );


      if (i < 2) {

        leds[i] =
          blendColor(
            leds[i],
            CRGB::White,
            pattern == 1
              ? 95
              : 75
          );
      }
    }


    if (
      extending &&
      millis() - ignition < 170 &&
      len
    ) {

      if (pattern == 1) {

        for (
          uint8_t i = 0;
          i < 10;
          i++
        ) {

          if (
            entropy.next8() > 120
          ) {

            leds[i] =
              blendColor(
                leds[i],
                CRGB::White,
                120
              );
          }
        }
      }

      else {

        leds[0] =
          blendColor(
            leds[0],
            CRGB::White,
            180
          );
      }
    }


    if (clash) {

      for (
        uint8_t i = 0;
        i < len;
        i++
      ) {

        if (
          entropy.next8() > 90
        ) {

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


    if (
      len &&
      entropy.next8() > 220
    ) {

      uint8_t p =
        entropy.next8() % len;


      leds[p] =
        blendColor(
          leds[p],
          CRGB::White,
          150
        );
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

  void enter() override {

    hue =
      entropy.next8();
  }


  void update(
    Gesture g
  ) override {

    if (g == G_TAP) {

      spd =
        (uint8_t)(
          spd % 10 + 1
        );
    }


    if (g == G_DOUBLE) {
      rev = !rev;
    }
  }


  void render() override {

    uint8_t s =
      (uint8_t)(
        spd *
        (globalSpeed + 40) /
        75
      );


    if (!s) {
      s = 1;
    }


    if (rev) {
      hue -= s;
    }

    else {
      hue += s;
    }


    fill_rainbow(
      leds,
      NUM_LEDS,
      hue,
      16
    );
  }
};


/* ================================================================
 * MODES
 *
 * EXACT HOME ORDER:
 *
 * 0 STATUS BALL
 * 1 LIGHTSABER
 * 2 RAINBOW
 * ================================================================ */

ModeStatus modeStatus;
ModeSaber modeSaber;
ModeRainbow modeRainbow;

Mode* mainModes[] = {
  &modeStatus,
  &modeSaber,
  &modeRainbow
};

const uint8_t MAIN_MODE_COUNT = 3;


/* ================================================================
 * FIREBALL ENGINE
 *
 * V1.6:
 * Bright projectile, tiny local glow, no bright trail.
 * ================================================================ */

class FireballEngine {

  float pos = 9.0f;
  uint8_t flicker = 0;

public:

  void reset() {

    pos = 9.0f;
    flicker = entropy.next8();
  }


  void update(
    float p
  ) {

    pos = p;
    flicker = entropy.next8();
  }


  void render() {

    /*
     * Do NOT retain previous pixels.
     */
    clearLeds();


    int p =
      (int)(
        pos + 0.5f
      );


    if (
      p < 0 ||
      p >= NUM_LEDS
    ) {
      return;
    }


    /*
     * Tiny dim local glow.
     */
    if (p > 0) {

      leds[p - 1] =
        CRGB(
          35,
          2,
          0
        );
    }


    if (p < 9) {

      leds[p + 1] =
        CRGB(
          45,
          4,
          0
        );
    }


    /*
     * Main fireball.
     */
    leds[p] =
      CRGB(
        255,
        (uint8_t)(
          40 +
          (flicker >> 3)
        ),
        2
      );


    leds[p] =
      blendColor(
        leds[p],
        CRGB(
          255,
          220,
          80
        ),
        140
      );
  }
};


/* ================================================================
 * FIREBALL GAME
 * ================================================================ */

class AppFireball {

  enum GameState : uint8_t {
    GAME_RUNNING,
    GAME_RESULT,
    GAME_DEAD
  };


  GameState state =
    GAME_RUNNING;


  uint8_t score = 0;
  uint8_t lives = 3;

  uint16_t travelTime = 165;

  float position = 9.0f;

  uint32_t lastUpdate = 0;
  uint32_t resultStart = 0;

  uint8_t resultType = 0;

  FireballEngine fireball;


  uint8_t shownScore() const {

    /*
     * Explicit clamp.
     *
     * NO min().
     */
    if (score > 10) {
      return 10;
    }

    return score;
  }


  void startRound() {

    position = 9.0f;

    fireball.reset();

    lastUpdate = millis();

    state =
      GAME_RUNNING;
  }


  void success() {

    if (score < 255) {
      score++;
    }


    resultType = 1;

    resultStart =
      millis();


    /*
     * Faster after each successful hit.
     */
    if (travelTime > 45) {

      travelTime =
        (uint16_t)(
          travelTime * 88UL / 100UL
        );
    }


    if (travelTime < 45) {
      travelTime = 45;
    }


    state =
      GAME_RESULT;
  }


  void failure() {

    if (lives > 0) {
      lives--;
    }


    resultType = 2;

    resultStart =
      millis();


    if (lives == 0) {
      state = GAME_DEAD;
    }

    else {
      state = GAME_RESULT;
    }
  }

public:

  void enter() {

    score = 0;
    lives = 3;

    travelTime = 165;

    resultType = 0;

    startRound();
  }


  void update(
    Gesture g
  ) {

    if (
      state == GAME_DEAD
    ) {

      if (g == G_TAP) {
        enter();
      }

      return;
    }


    if (
      state == GAME_RESULT
    ) {

      if (g == G_TAP) {
        startRound();
      }

      return;
    }


    if (
      state == GAME_RUNNING &&
      g == G_TAP
    ) {

      /*
       * Tight hit zone near LED 0.
       */
      if (position <= 1.45f) {
        success();
      }

      else {
        failure();
      }
    }
  }


  void render() {

    clearLeds();


    /* RUNNING */

    if (
      state == GAME_RUNNING
    ) {

      uint32_t now =
        millis();


      uint32_t elapsed =
        now -
        lastUpdate;


      lastUpdate = now;


      position -=
        (float)elapsed /
        (float)travelTime;


      fireball.update(
        position
      );


      fireball.render();


      /*
       * Very dim target indication.
       */
      if (position > 1.45f) {

        leds[0] =
          CRGB(
            0,
            30,
            7
          );

        leds[1] =
          CRGB(
            0,
            15,
            4
          );
      }


      /*
       * Missed the target.
       */
      if (position < -0.8f) {

        failure();
      }


      return;
    }


    /* RESULT */

    if (
      state == GAME_RESULT
    ) {

      uint32_t age =
        millis() -
        resultStart;


      if (resultType == 1) {

        /*
         * Success pulse.
         */
        uint8_t pulse =
          beatsin8(
            8,
            50,
            255
          );


        for (
          uint8_t i = 0;
          i < 10;
          i++
        ) {

          leds[i] =
            CRGB(
              0,
              pulse,
              pulse / 5
            );
        }
      }

      else {

        /*
         * Miss pulse.
         */
        uint8_t pulse =
          beatsin8(
            10,
            20,
            190
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


      /*
       * Show score after feedback.
       */
      if (age > 350) {

        clearLeds();


        uint8_t shown =
          shownScore();


        for (
          uint8_t i = 0;
          i < shown;
          i++
        ) {

          leds[i] =
            CRGB(
              0,
              110,
              25
            );
        }
      }


      return;
    }


    /* GAME OVER */

    uint8_t pulse =
      beatsin8(
        5,
        35,
        190
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


    /*
     * Score stays visible.
     */
    uint8_t shown =
      shownScore();


    for (
      uint8_t i = 0;
      i < shown;
      i++
    ) {

      leds[i] =
        CRGB(
          0,
          100,
          20
        );
    }
  }
};


AppFireball appFireball;


/* ================================================================
 * MENU ACTIVITY
 * ================================================================ */

void touchMenu() {
  menuLastActivity = millis();
}


bool inAnyMenu() {

  return
    uiState == UI_MENU_MAIN ||
    uiState == UI_MENU_GAMES ||
    uiState == UI_MENU_SETTINGS ||
    uiState == UI_BRIGHTNESS ||
    uiState == UI_SPEED ||
    uiState == UI_INFO;
}


/* ================================================================
 * HOME MENU
 *
 * PHYSICAL LED LAYOUT:
 *
 *  LED 0 = BALL
 *  LED 1 = SABER
 *  LED 2 = RAINBOW
 *  LED 3 = GAMES
 *  LED 4 = SETTINGS
 *
 * ALL FIVE ARE NOW DIRECTLY ADJACENT.
 *
 * 1  2  3  4  5
 * ●  ●  ●  ●  ●
 *
 * BALL/SABER/RAINBOW = CACTUS GREEN
 * GAMES               = V1.5 ORANGE
 * SETTINGS            = V1.5 BLUE
 * ================================================================ */

void renderMainMenu() {

  clearLeds();


  /*
   * 1 — STATUS / BLUE BALL
   * Normal portal color = cactus green.
   */
  leds[0] =
    COLOR_CACTUS;


  /*
   * 2 — LIGHTSABER
   */
  leds[1] =
    COLOR_CACTUS;


  /*
   * 3 — RAINBOW
   */
  leds[2] =
    COLOR_CACTUS;


  /*
   * 4 — GAMES
   * EXACT V1.5 portal color.
   */
  leds[3] =
    COLOR_GAMES;


  /*
   * 5 — SETTINGS
   * EXACT V1.5 portal color.
   */
  leds[4] =
    COLOR_SETTINGS;


  /*
   * Remaining physical LEDs are OFF.
   * This is the right-side cutoff.
   */
  leds[5] = CRGB::Black;
  leds[6] = CRGB::Black;
  leds[7] = CRGB::Black;
  leds[8] = CRGB::Black;
  leds[9] = CRGB::Black;


  /*
   * Universal cursor.
   */
  renderCursor(
    mainCursor,
    210
  );
}


/* ================================================================
 * GAMES MENU
 *
 * Preserve V1.5 underlying colors:
 *   BACK     = orange
 *   FIREBALL = red
 *
 * Cursor = hot pink.
 * ================================================================ */

void renderGamesMenu() {

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


  renderCursor(
    gamesCursor,
    205
  );
}


/* ================================================================
 * SETTINGS MENU
 *
 * PRESERVED V1.5 COLORS:
 *
 * 0 BACK       = blue
 * 1 BRIGHTNESS = green
 * 2 SPEED      = green
 * 3 INFO       = green
 * 4 BOOTSEL    = yellow
 * 5 SHUTDOWN   = red
 *
 * SELECTED ITEM = ALWAYS HOT PINK.
 * ================================================================ */

const uint8_t SETTINGS_COUNT = 6;


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


  leds[4] =
    CRGB(
      180,
      120,
      0
    );


  leds[5] =
    CRGB(
      255,
      0,
      0
    );


  /*
   * ONE universal cursor rule:
   * selected = hot pink.
   */
  renderCursor(
    settingsCursor,
    205
  );
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

uint8_t brightLevel = 0;


void renderBrightness() {

  for (
    uint8_t i = 0;
    i < 10;
    i++
  ) {

    leds[i] =
      (
        i <= brightLevel
      )
      ? COLOR_HOT_PINK
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

  for (
    uint8_t i = 0;
    i < 10;
    i++
  ) {

    leds[i] =
      (
        i <= speedLevel
      )
      ? COLOR_HOT_PINK
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

    uint8_t n =
      (uint8_t)(
        (millis() / 1000UL) % 11UL
      );


    for (
      uint8_t i = 0;
      i < 10;
      i++
    ) {

      leds[i] =
        (
          i < n
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
 * SHUTDOWN
 * ================================================================ */

bool sleepWaitRelease = true;
uint32_t sleepStart = 0;


void enterShutdown() {

  uiState =
    UI_SLEEP;

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

    if (
      digitalRead(
        BUTTON_PIN
      ) == HIGH
    ) {

      sleepWaitRelease = false;
    }

    return;
  }


  if (
    digitalRead(
      BUTTON_PIN
    ) == LOW
  ) {

    if (!sleepStart) {
      sleepStart = millis();
    }


    if (
      millis() -
      sleepStart >= 1500
    ) {

      sleepStart = 0;

      applyBrightness();

      /*
       * Return to original V1.5 default idle.
       */
      uiState = UI_MODE;
      currentMainMode = 0;

      modeStatus.enter();

      sleepWaitRelease = true;
    }

  }

  else {

    sleepStart = 0;
  }
}


/* ================================================================
 * USB BOOTSEL
 * ================================================================ */

void enterBootsel() {

  clearLeds();

  FastLED.setBrightness(0);
  FastLED.show();

  delay(40);

  /*
   * RP2350 ROM USB bootloader.
   */
  reset_usb_boot(
    0,
    0
  );


  /*
   * Defensive fallback.
   */
  while (true) {
    delay(1000);
  }
}


/* ================================================================
 * RETURN TO DEFAULT IDLE
 * ================================================================ */

void returnToIdle() {

  uiState =
    UI_MODE;

  currentMainMode = 0;

  modeStatus.enter();
}


/* ================================================================
 * UI HANDLER
 * ================================================================ */

void handleUI(
  Gesture g
) {

  if (g == G_NONE) {
    return;
  }


  if (inAnyMenu()) {
    touchMenu();
  }


  /*
   * VERY LONG HOLD:
   * Return directly to blue bouncing ball.
   */

  if (g == G_VERY_LONG) {

    fill_solid(
      leds,
      NUM_LEDS,
      COLOR_HOT_PINK
    );

    FastLED.show();

    delay(160);

    returnToIdle();

    return;
  }


  /* ==============================================================
   * FIREBALL GAME
   * ============================================================== */

  if (
    uiState == UI_IN_GAME
  ) {

    if (
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_GAMES;

      gamesCursor = 0;

      touchMenu();

      return;
    }


    appFireball.update(g);

    return;
  }


  /* ==============================================================
   * NORMAL MODE
   * ============================================================== */

  if (
    uiState == UI_MODE
  ) {

    /*
     * HOLD = enter home menu.
     */
    if (
      g == G_HOLD
    ) {

      /*
       * The normal mode maps directly to the
       * first three home positions.
       */
      mainCursor =
        currentMainMode;

      uiState =
        UI_MENU_MAIN;

      touchMenu();

      return;
    }


    /*
     * LONG HOLD = default blue ball.
     */
    if (
      g == G_LONG_HOLD
    ) {

      returnToIdle();

      return;
    }


    /*
     * Pass gestures into active mode.
     *
     * Status has no special tap action,
     * so it simply remains the bouncing ball.
     */
    mainModes[
      currentMainMode
    ]->update(g);

    return;
  }


  /* ==============================================================
   * MAIN HOME MENU
   * ============================================================== */

  if (
    uiState == UI_MENU_MAIN
  ) {

    /*
     * TAP = next portal
     *
     * 0 Ball
     * 1 Saber
     * 2 Rainbow
     * 3 Games
     * 4 Settings
     */
    if (
      g == G_TAP
    ) {

      mainCursor =
        (uint8_t)(
          (mainCursor + 1) % 5
        );

      return;
    }


    /*
     * DOUBLE TAP = previous portal
     */
    if (
      g == G_DOUBLE
    ) {

      if (mainCursor == 0) {
        mainCursor = 4;
      }

      else {
        mainCursor--;
      }

      return;
    }


    /*
     * HOLD = select.
     */
    if (
      g == G_HOLD
    ) {

      switch (
        mainCursor
      ) {

        /* 1 — STATUS BALL */

        case 0:

          currentMainMode = 0;

          modeStatus.enter();

          uiState =
            UI_MODE;

          return;


        /* 2 — LIGHTSABER */

        case 1:

          currentMainMode = 1;

          modeSaber.enter();

          uiState =
            UI_MODE;

          return;


        /* 3 — RAINBOW */

        case 2:

          currentMainMode = 2;

          modeRainbow.enter();

          uiState =
            UI_MODE;

          return;


        /* 4 — GAMES */

        case 3:

          gamesCursor = 0;

          uiState =
            UI_MENU_GAMES;

          touchMenu();

          return;


        /* 5 — SETTINGS */

        case 4:

          settingsCursor = 0;

          uiState =
            UI_MENU_SETTINGS;

          touchMenu();

          return;
      }
    }


    /*
     * LONG HOLD = back to idle.
     */
    if (
      g == G_LONG_HOLD
    ) {

      returnToIdle();

      return;
    }
  }


  /* ==============================================================
   * GAMES MENU
   * ============================================================== */

  if (
    uiState == UI_MENU_GAMES
  ) {

    /*
     * TAP = next
     */
    if (
      g == G_TAP
    ) {

      gamesCursor =
        (uint8_t)(
          (gamesCursor + 1) % 2
        );

      return;
    }


    /*
     * DOUBLE = previous
     */
    if (
      g == G_DOUBLE
    ) {

      if (gamesCursor == 0) {
        gamesCursor = 1;
      }

      else {
        gamesCursor = 0;
      }

      return;
    }


    /*
     * HOLD = select
     */
    if (
      g == G_HOLD
    ) {

      /*
       * BACK
       */
      if (
        gamesCursor == 0
      ) {

        uiState =
          UI_MENU_MAIN;

        mainCursor = 3;

        touchMenu();

        return;
      }


      /*
       * FIREBALL
       */
      if (
        gamesCursor == 1
      ) {

        appFireball.enter();

        uiState =
          UI_IN_GAME;

        touchMenu();

        return;
      }
    }


    /*
     * LONG HOLD = back to main.
     */
    if (
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_MAIN;

      mainCursor = 3;

      touchMenu();

      return;
    }
  }


  /* ==============================================================
   * SETTINGS MENU
   * ============================================================== */

  if (
    uiState == UI_MENU_SETTINGS
  ) {

    /*
     * TAP = next
     */
    if (
      g == G_TAP
    ) {

      settingsCursor =
        (uint8_t)(
          (settingsCursor + 1) %
          SETTINGS_COUNT
        );

      return;
    }


    /*
     * DOUBLE = previous
     */
    if (
      g == G_DOUBLE
    ) {

      if (settingsCursor == 0) {
        settingsCursor =
          SETTINGS_COUNT - 1;
      }

      else {
        settingsCursor--;
      }

      return;
    }


    /*
     * HOLD = select
     */
    if (
      g == G_HOLD
    ) {

      switch (
        settingsCursor
      ) {

        /* BACK */

        case 0:

          uiState =
            UI_MENU_MAIN;

          mainCursor = 4;

          touchMenu();

          return;


        /* BRIGHTNESS */

        case 1: {

          uint8_t best = 255;


          for (
            uint8_t i = 0;
            i < 10;
            i++
          ) {

            uint8_t d =
              (uint8_t)abs(
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

          return;
        }


        /* SPEED */

        case 2:

          uiState =
            UI_SPEED;

          touchMenu();

          return;


        /* INFO */

        case 3:

          infoPage = 0;

          uiState =
            UI_INFO;

          touchMenu();

          return;


        /* BOOTSEL */

        case 4:

          enterBootsel();

          return;


        /* SHUTDOWN */

        case 5:

          enterShutdown();

          return;
      }
    }


    /*
     * LONG HOLD = back to main.
     */
    if (
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_MAIN;

      mainCursor = 4;

      touchMenu();

      return;
    }
  }


  /* ==============================================================
   * BRIGHTNESS
   * ============================================================== */

  if (
    uiState == UI_BRIGHTNESS
  ) {

    if (
      g == G_TAP
    ) {

      brightLevel =
        (uint8_t)(
          (brightLevel + 1) % 10
        );


      globalBrightness =
        brightVals[
          brightLevel
        ];


      applyBrightness();

      return;
    }


    if (
      g == G_DOUBLE
    ) {

      if (brightLevel == 0) {
        brightLevel = 9;
      }

      else {
        brightLevel--;
      }


      globalBrightness =
        brightVals[
          brightLevel
        ];


      applyBrightness();

      return;
    }


    if (
      g == G_HOLD ||
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_SETTINGS;

      settingsCursor = 1;

      touchMenu();

      return;
    }
  }


  /* ==============================================================
   * SPEED
   * ============================================================== */

  if (
    uiState == UI_SPEED
  ) {

    if (
      g == G_TAP
    ) {

      speedLevel =
        (uint8_t)(
          (speedLevel + 1) % 10
        );


      globalSpeed =
        speedVals[
          speedLevel
        ];

      return;
    }


    if (
      g == G_DOUBLE
    ) {

      if (speedLevel == 0) {
        speedLevel = 9;
      }

      else {
        speedLevel--;
      }


      globalSpeed =
        speedVals[
          speedLevel
        ];

      return;
    }


    if (
      g == G_HOLD ||
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_SETTINGS;

      settingsCursor = 2;

      touchMenu();

      return;
    }
  }


  /* ==============================================================
   * INFO
   * ============================================================== */

  if (
    uiState == UI_INFO
  ) {

    if (
      g == G_TAP
    ) {

      infoPage =
        (uint8_t)(
          (infoPage + 1) % 3
        );

      return;
    }


    if (
      g == G_DOUBLE
    ) {

      if (infoPage == 0) {
        infoPage = 2;
      }

      else {
        infoPage--;
      }

      return;
    }


    if (
      g == G_HOLD ||
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_SETTINGS;

      settingsCursor = 3;

      touchMenu();

      return;
    }
  }
}


/* ================================================================
 * RENDER UI
 * ================================================================ */

void renderUI() {

  switch (
    uiState
  ) {

    case UI_MODE:

      mainModes[
        currentMainMode
      ]->render();

      break;


    case UI_MENU_MAIN:

      renderMainMenu();

      break;


    case UI_MENU_GAMES:

      renderGamesMenu();

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


    case UI_IN_GAME:

      appFireball.render();

      break;


    case UI_SLEEP:

      clearLeds();

      break;
  }
}


/* ================================================================
 * SETUP
 * ================================================================ */

void setup() {

  FastLED.addLeds<
    WS2812B,
    LED_PIN,
    GRB
  >(
    leds,
    NUM_LEDS
  );


  FastLED.setDither(0);

  applyBrightness();

  FastLED.clear(true);

  entropy.begin();

  button.begin();


  /*
   * ORIGINAL V1.5 STARTUP ANIMATION
   */
  for (
    uint8_t i = 0;
    i < 10;
    i++
  ) {

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


  /*
   * DEFAULT IDLE = ORIGINAL BLUE BALL
   */
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

  uint32_t now =
    millis();


  /* SHUTDOWN */

  if (
    uiState == UI_SLEEP
  ) {

    updateShutdown();

    delay(5);

    return;
  }


  /* ENTROPY */

  entropy.update();


  /* MENU TIMEOUT */

  if (
    inAnyMenu() &&
    now - menuLastActivity >
      MENU_TIMEOUT_MS
  ) {

    returnToIdle();
  }


  /* BUTTON */

  Gesture g =
    button.poll(
      inAnyMenu() ||
      uiState == UI_IN_GAME
    );


  if (
    g != G_NONE
  ) {

    handleUI(g);
  }


  /* FRAME */

  if (
    now - lastFrame >= FRAME_MS
  ) {

    lastFrame = now;

    renderUI();

    FastLED.show();
  }
}