#include <Arduino.h>
#include <FastLED.h>
#include "pico/bootrom.h"

/*
 * ================================================================
 * LAMOKA1 DEV BRAIN — PORTAL INTERFACE ENGINE V1.7
 * ================================================================
 *
 * HOME
 * ----
 * 1 = BLUE STATUS BALL
 * 2 = LIGHTSABER
 * 3 = RAINBOW
 * 4 = GAMES
 * 5 = SETTINGS
 *
 * HOME COLORS
 * -----------
 * STATUS / SABER / RAINBOW = EXACT SAME CACTUS GREEN
 * GAMES                     = V1.5 ORANGE
 * SETTINGS                  = V1.5 BLUE
 *
 * CURSOR
 * ------
 * EVERY selected item is HOT PINK.
 *
 * DEFAULT IDLE
 * ------------
 * Original V1.5 blue bouncing ball.
 *
 * BRIGHTNESS
 * ----------
 * Minimum = 6
 *
 * FIREBALL
 * --------
 * Based directly on the standalone Fireball game supplied by Bear.
 *
 * - Black background
 * - One green D1 target
 * - White center
 * - Hot/yellow/orange/red trail
 * - Right -> left movement
 * - Quick press at D1 = catch
 * - Successful catch = blast + score + faster next round
 * - Miss = quick red flash + immediate refire
 * - Binary score display
 * - Three-speed difficulty range
 *
 * PIE INTEGRATION
 * ---------------
 * PIE retains its normal menu/button architecture.
 * A quick press inside Fireball attempts the catch.
 * Long hold still exits the game back to Games.
 *
 * BOOTSEL
 * -------
 * reset_usb_boot(0, 0)
 *
 * IMPORTANT
 * ---------
 * No min() calls are used anywhere.
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

#define DEFAULT_BRIGHTNESS   6
#define DEFAULT_SPEED      100

#define FRAME_MS            16
#define MENU_TIMEOUT_MS     16000UL


CRGB leds[NUM_LEDS];


/* ================================================================
 * COLORS
 * ================================================================ */

/*
 * ONE UI GREEN.
 *
 * Every green UI/menu/status indicator uses this exact color.
 */
const CRGB COLOR_CACTUS =
  CRGB(0, 90, 18);


/*
 * Universal cursor.
 */
const CRGB COLOR_HOT_PINK =
  CRGB(255, 40, 160);


/*
 * V1.5 Games portal.
 */
const CRGB COLOR_GAMES =
  CRGB(255, 90, 0);


/*
 * V1.5 Settings portal.
 */
const CRGB COLOR_SETTINGS =
  CRGB(0, 50, 100);


/*
 * Fireball palette.
 */
const CRGB FIRE_GREEN =
  CRGB(0, 180, 40);

const CRGB FIRE_RED =
  CRGB(150, 0, 0);

const CRGB FIRE_ORANGE =
  CRGB(255, 45, 0);

const CRGB FIRE_HOT =
  CRGB(255, 130, 10);

const CRGB FIRE_WHITE =
  CRGB(255, 255, 255);


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

uint8_t globalBrightness =
  DEFAULT_BRIGHTNESS;

uint8_t globalSpeed =
  DEFAULT_SPEED;


/* ================================================================
 * UI GLOBALS
 * ================================================================ */

/*
 * HOME
 *
 * 0 = STATUS BALL
 * 1 = LIGHTSABER
 * 2 = RAINBOW
 * 3 = GAMES
 * 4 = SETTINGS
 */
uint8_t mainCursor = 0;


/*
 * GAMES
 *
 * 0 = BACK
 * 1 = FIREBALL
 */
uint8_t gamesCursor = 0;


/*
 * SETTINGS
 *
 * 0 = BACK
 * 1 = BRIGHTNESS
 * 2 = SPEED
 * 3 = INFO
 * 4 = BOOTSEL
 * 5 = SHUTDOWN
 */
uint8_t settingsCursor = 0;


/*
 * Current normal mode:
 *
 * 0 = STATUS
 * 1 = SABER
 * 2 = RAINBOW
 */
uint8_t currentMainMode = 0;


/*
 * Settings submenu return point.
 */
uint8_t settingsReturnCursor = 0;


/*
 * Menu timeout.
 */
uint32_t menuLastActivity = 0;


/*
 * UI state.
 */
UIState uiState = UI_MODE;


/* ================================================================
 * LED HELPERS
 * ================================================================ */

void applyBrightness() {

  uint8_t b =
    globalBrightness;

  if (b < 6) {
    b = 6;
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
  uint8_t amount
) {

  return CRGB(
    lerp8by8(a.r, b.r, amount),
    lerp8by8(a.g, b.g, amount),
    lerp8by8(a.b, b.b, amount)
  );
}


/*
 * Universal hot-pink cursor.
 */
void renderCursor(
  uint8_t index,
  uint8_t minimumBrightness = 180
) {

  if (index >= NUM_LEDS) {
    return;
  }

  CRGB c =
    COLOR_HOT_PINK;

  c.nscale8(
    beatsin8(
      7,
      minimumBrightness,
      255
    )
  );

  leds[index] =
    c;
}


/* ================================================================
 * ENTROPY SERVICE
 * ================================================================ */

class EntropyService {

  uint32_t s = 1;

  void mix() {

    uint32_t a =
      analogRead(ADC0);

    uint32_t b =
      analogRead(ADC1);

    uint32_t c =
      analogRead(ADC2);

    uint32_t d =
      analogRead(ADC3);


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

    pinMode(
      COLOR_ENTROPY_PIN,
      INPUT
    );

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

    return (uint8_t)(
      next() & 255UL
    );
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

    return (
      next() & 1UL
    ) != 0;
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


    if (
      raw != lastRaw
    ) {

      debounceStart =
        now;

      lastRaw =
        raw;
    }


    if (
      now - debounceStart < 28
    ) {

      return G_NONE;
    }


    bool pressed =
      !raw;


    /*
     * NEW PRESS
     */
    if (
      pressed &&
      !wasPressed
    ) {

      wasPressed = true;

      pressStart =
        now;

      holdFired = false;
      longFired = false;
      veryLongFired = false;

      return G_NONE;
    }


    /*
     * HOLDING
     */
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


    /*
     * RELEASE
     */
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

        /*
         * Menus need immediate taps.
         */
        if (inMenu) {

          return G_TAP;
        }


        /*
         * Outside menus, determine single
         * versus double tap.
         */
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


    /*
     * Delayed single tap.
     */
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
 * STATUS — ORIGINAL BLUE BOUNCING BALL
 * ================================================================ */

class ModeStatus : public Mode {

  int8_t p = 0;
  int8_t d = 1;

  uint32_t last = 0;

public:

  void enter() override {

    p = 0;
    d = 1;

    last =
      millis();
  }


  void render() override {

    clearLeds();


    /*
     * Preserve the original V1.5 timing.
     */
    uint16_t iv =
      (uint16_t)(
        28 +
        8000 /
        (globalSpeed + 50)
      );


    if (
      millis() - last >= iv
    ) {

      last =
        millis();

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
     * Original blue moving trail.
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

    ignition =
      millis();
  }

public:

  void enter() override {

    launch();
  }


  void update(
    Gesture g
  ) override {

    if (
      g == G_TAP
    ) {

      if (extending) {

        extending = false;
      }

      else {

        launch();
      }
    }


    if (
      g == G_DOUBLE
    ) {

      launch();
    }


    if (
      g == G_HOLD
    ) {

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


    if (
      pattern == 1
    ) {

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
          (uint8_t)(
            i * 11
          )
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


      if (
        pattern == 4
      ) {

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


      if (
        pattern == 2
      ) {

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


      if (
        i < 2
      ) {

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


    /*
     * Ignition.
     */
    if (
      extending &&
      millis() - ignition < 170 &&
      len
    ) {

      leds[0] =
        blendColor(
          leds[0],
          CRGB::White,
          180
        );
    }


    /*
     * Clash.
     */
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


    /*
     * Spark.
     */
    if (
      len &&
      entropy.next8() > 220
    ) {

      uint8_t p =
        entropy.next8() %
        len;


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

    if (
      g == G_TAP
    ) {

      spd =
        (uint8_t)(
          spd % 10 + 1
        );
    }


    if (
      g == G_DOUBLE
    ) {

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
 * MODE INSTANCES
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
 * FIREBALL GAME
 *
 * This is based on the standalone game supplied by Bear.
 *
 * Physical representation:
 *
 * LED 0 = D1 target
 * LED 9 = fireball start
 *
 * Visual:
 *
 * WHITE  = projectile center
 * HOT    = immediate trail
 * ORANGE = secondary trail
 * RED    = outer trail
 *
 * The supplied game uses a real fire trail, so V1.7 uses that
 * instead of the earlier compact-projectile design.
 * ================================================================ */

class AppFireball {

  /*
   * Standalone-game constants.
   */
  static const int TARGET_LED = 0;

  static const int START_SPEED = 115;
  static const int SPEEDUP = 10;
  static const int MIN_SPEED = 30;

  /*
   * Game states.
   */
  enum GameState : uint8_t {
    GAME_RUNNING,
    GAME_SCORE,
    GAME_MISSED,
    GAME_OVER
  };


  GameState state =
    GAME_RUNNING;


  /*
   * Standalone game values.
   */
  bool gameRunning = false;

  bool scoreShowing = false;

  uint32_t lastMove = 0;

  int firePosition =
    NUM_LEDS - 1;

  int fireSpeed =
    START_SPEED;

  uint16_t score = 0;

  uint32_t stateTime = 0;

  /*
   * Used to keep score/lives feedback without
   * blocking the main PIE loop.
   */
  uint8_t lives = 3;


  /*
   * Explicit score clamp.
   * No min().
   */
  uint8_t displayedScore() const {

    if (score > 10) {
      return 10;
    }

    return (uint8_t)score;
  }


  /*
   * Draw the exact Fireball game.
   */
  void drawGame() {

    clearLeds();


    /*
     * D1 target.
     *
     * Exactly one green dot.
     */
    leds[TARGET_LED] =
      FIRE_GREEN;


    /*
     * White fireball center.
     */
    if (
      firePositionValid()
    ) {

      leds[
        firePosition
      ] =
        FIRE_WHITE;
    }


    /*
     * Hot trail.
     */
    if (
      firePosition + 1 < NUM_LEDS
    ) {

      leds[
        firePosition + 1
      ] =
        FIRE_HOT;
    }


    /*
     * Orange trail.
     */
    if (
      firePosition + 2 < NUM_LEDS
    ) {

      leds[
        firePosition + 2
      ] =
        FIRE_ORANGE;
    }


    /*
     * Red trail.
     */
    if (
      firePosition + 3 < NUM_LEDS
    ) {

      leds[
        firePosition + 3
      ] =
        FIRE_RED;
    }


    FastLED.show();
  }


  bool firePositionValid() const {

    return (
      firePosition >= 0 &&
      firePosition < NUM_LEDS
    );
  }


  /*
   * Non-blocking success blast.
   *
   * This is the same visual concept as the supplied game,
   * but implemented as a state machine so PIE remains responsive.
   */
  void renderSuccessBlast() {

    clearLeds();


    uint32_t elapsed =
      millis() -
      stateTime;


    uint8_t frame =
      (uint8_t)(
        elapsed / 45UL
      );


    if (frame > 7) {
      frame = 7;
    }


    for (
      int i = 0;
      i < NUM_LEDS;
      i++
    ) {

      int distance =
        abs(
          i - TARGET_LED
        );


      if (
        distance <= frame
      ) {

        int level =
          255 -
          distance * 50 -
          frame * 18;


        if (level < 10) {
          level = 10;
        }


        leds[i] =
          CRGB(
            (uint8_t)level,
            (uint8_t)(level / 2),
            (uint8_t)(level / 12)
          );
      }
    }


    FastLED.show();
  }


  /*
   * Score screen.
   *
   * Binary score, exactly as supplied.
   */
  void renderScore() {

    clearLeds();


    uint8_t shown =
      displayedScore();


    for (
      uint8_t i = 0;
      i < 10;
      i++
    ) {

      if (
        shown &
        (1U << i)
      ) {

        leds[i] =
          COLOR_CACTUS;
      }
    }


    FastLED.show();
  }


  /*
   * Quick red miss flash.
   */
  void renderMissFlash() {

    clearLeds();


    uint32_t elapsed =
      millis() -
      stateTime;


    if (
      elapsed < 70
    ) {

      fill_solid(
        leds,
        NUM_LEDS,
        FIRE_RED
      );
    }


    FastLED.show();
  }


  /*
   * Start a completely new fireball.
   */
  void refire() {

    firePosition =
      NUM_LEDS - 1;

    lastMove =
      millis();

    gameRunning = true;

    state =
      GAME_RUNNING;
  }


  /*
   * Successful catch.
   */
  void catchFireball() {

    score++;


    /*
     * Speed up the next round.
     */
    fireSpeed -=
      SPEEDUP;


    if (
      fireSpeed < MIN_SPEED
    ) {

      fireSpeed =
        MIN_SPEED;
    }


    /*
     * Start impact blast.
     */
    stateTime =
      millis();


    state =
      GAME_SCORE;
  }


  /*
   * Miss.
   */
  void missedFireball() {

    if (
      lives > 0
    ) {

      lives--;
    }


    if (
      lives == 0
    ) {

      state =
        GAME_OVER;

      stateTime =
        millis();

      return;
    }


    /*
     * Fast red flash first.
     */
    stateTime =
      millis();

    state =
      GAME_MISSED;
  }


public:

  /*
   * Enter Fireball.
   */
  void enter() {

    gameRunning = true;

    score = 0;

    lives = 3;

    fireSpeed =
      START_SPEED;

    firePosition =
      NUM_LEDS - 1;

    lastMove =
      millis();

    state =
      GAME_RUNNING;

    stateTime =
      millis();

    drawGame();
  }


  /*
   * Exit Fireball.
   */
  void exit() {

    gameRunning = false;

    clearLeds();

    FastLED.show();
  }


  /*
   * Quick press = catch attempt.
   *
   * Long hold never reaches this routine because PIE
   * handles Long Hold as the Game -> Games navigation.
   */
  void update(
    Gesture g
  ) {

    /*
     * GAME OVER
     *
     * Tap restarts.
     */
    if (
      state == GAME_OVER
    ) {

      if (
        g == G_TAP
      ) {

        enter();
      }

      return;
    }


    /*
     * SUCCESS STATE
     *
     * Do not move the projectile.
     */
    if (
      state == GAME_SCORE
    ) {

      return;
    }


    /*
     * MISS STATE
     */
    if (
      state == GAME_MISSED
    ) {

      /*
       * After the red flash, immediately refire.
       */
      if (
        millis() -
        stateTime >= 140
      ) {

        refire();
      }

      return;
    }


    /*
     * RUNNING
     */
    if (
      state == GAME_RUNNING &&
      g == G_TAP
    ) {

      /*
       * Exact standalone behavior:
       *
       * Only catch when the fireball is actually at D1.
       */
      if (
        firePosition ==
        TARGET_LED
      ) {

        catchFireball();
      }
    }
  }


  /*
   * Called from renderUI every frame.
   */
  void render() {

    /*
     * RUNNING
     */
    if (
      state == GAME_RUNNING
    ) {

      uint32_t now =
        millis();


      /*
       * Move the fireball only when
       * enough time has elapsed.
       */
      if (
        now - lastMove >=
        (uint32_t)fireSpeed
      ) {

        lastMove =
          now;

        firePosition--;


        /*
         * It passed D1.
         */
        if (
          firePosition <
          TARGET_LED
        ) {

          missedFireball();

          /*
           * Do not return yet.
           * Draw the miss flash.
           */
        }
      }


      /*
       * Game is still running.
       */
      if (
        state == GAME_RUNNING
      ) {

        drawGame();
      }

      else {

        renderMissFlash();
      }


      return;
    }


    /*
     * SUCCESS / BLAST
     */
    if (
      state == GAME_SCORE
    ) {

      uint32_t elapsed =
        millis() -
        stateTime;


      /*
       * 8 blast frames.
       */
      if (
        elapsed < 360
      ) {

        renderSuccessBlast();

        return;
      }


      /*
       * Score screen for 750 ms.
       */
      if (
        elapsed < 1110
      ) {

        renderScore();

        return;
      }


      /*
       * Then new fireball.
       */
      refire();

      drawGame();

      return;
    }


    /*
     * MISS
     */
    if (
      state == GAME_MISSED
    ) {

      renderMissFlash();

      return;
    }


    /*
     * GAME OVER
     */
    if (
      state == GAME_OVER
    ) {

      /*
       * Red breathing indication.
       */
      uint8_t pulse =
        beatsin8(
          4,
          30,
          180
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
       * Binary score remains visible in cactus green.
       */
      uint8_t shown =
        displayedScore();


      for (
        uint8_t i = 0;
        i < 10;
        i++
      ) {

        if (
          shown &
          (1U << i)
        ) {

          leds[i] =
            COLOR_CACTUS;
        }
      }


      FastLED.show();

      return;
    }
  }
};


AppFireball appFireball;


/* ================================================================
 * MENU HELPERS
 * ================================================================ */

void touchMenu() {

  menuLastActivity =
    millis();
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
 * MAIN HOME MENU
 *
 * LED 0 = BALL       CACTUS
 * LED 1 = SABER      CACTUS
 * LED 2 = RAINBOW    CACTUS
 * LED 3 = GAMES      ORANGE
 * LED 4 = SETTINGS   BLUE
 *
 * LED 5-9 = OFF
 * ================================================================ */

void renderMainMenu() {

  clearLeds();


  /*
   * EXACT SAME GREEN.
   */
  leds[0] =
    COLOR_CACTUS;

  leds[1] =
    COLOR_CACTUS;

  leds[2] =
    COLOR_CACTUS;


  /*
   * V1.5 portal colors.
   */
  leds[3] =
    COLOR_GAMES;

  leds[4] =
    COLOR_SETTINGS;


  /*
   * Right-side cutoff.
   */
  for (
    uint8_t i = 5;
    i < 10;
    i++
  ) {

    leds[i] =
      CRGB::Black;
  }


  /*
   * Universal cursor.
   */
  renderCursor(
    mainCursor,
    180
  );
}


/* ================================================================
 * GAMES MENU
 * ================================================================ */

void renderGamesMenu() {

  clearLeds();


  /*
   * Preserve V1.5 game menu colors.
   */
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
    180
  );
}


/* ================================================================
 * SETTINGS MENU
 *
 * All green entries use exactly COLOR_CACTUS.
 * ================================================================ */

const uint8_t SETTINGS_COUNT = 6;


void renderSettingsMenu() {

  clearLeds();


  /*
   * BACK — V1.5 blue.
   */
  leds[0] =
    COLOR_SETTINGS;


  /*
   * BRIGHTNESS — exact cactus green.
   */
  leds[1] =
    COLOR_CACTUS;


  /*
   * SPEED — exact cactus green.
   */
  leds[2] =
    COLOR_CACTUS;


  /*
   * INFO — exact cactus green.
   */
  leds[3] =
    COLOR_CACTUS;


  /*
   * BOOTSEL — yellow.
   */
  leds[4] =
    CRGB(
      180,
      120,
      0
    );


  /*
   * SHUTDOWN — red.
   */
  leds[5] =
    CRGB(
      255,
      0,
      0
    );


  /*
   * Universal cursor.
   */
  renderCursor(
    settingsCursor,
    180
  );
}


/* ================================================================
 * BRIGHTNESS
 * ================================================================ */

const uint8_t brightVals[10] = {
  6,
  12,
  20,
  30,
  42,
  56,
  72,
  88,
  106,
  125
};


uint8_t brightLevel = 0;


void renderBrightness() {

  for (
    uint8_t i = 0;
    i < 10;
    i++
  ) {

    /*
     * Active level = pink.
     * Inactive = exact cactus green.
     */
    leds[i] =
      (
        i <= brightLevel
      )
      ? COLOR_HOT_PINK
      : COLOR_CACTUS;
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
      : COLOR_CACTUS;
  }
}


/* ================================================================
 * INFO
 * ================================================================ */

uint8_t infoPage = 0;


void renderInfo() {

  clearLeds();


  /*
   * Green uses exact UI cactus color.
   */
  if (
    infoPage == 0
  ) {

    fill_solid(
      leds,
      NUM_LEDS,
      COLOR_CACTUS
    );


    leds[0] =
      COLOR_HOT_PINK;
  }


  else if (
    infoPage == 1
  ) {

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
        ? COLOR_CACTUS
        : CRGB::Black;
    }
  }


  else {

    fill_solid(
      leds,
      NUM_LEDS,
      COLOR_CACTUS
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


  if (
    sleepWaitRelease
  ) {

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
      sleepStart =
        millis();
    }


    if (
      millis() -
      sleepStart >= 1500
    ) {

      sleepStart = 0;

      applyBrightness();

      uiState =
        UI_MODE;

      currentMainMode = 0;

      mainCursor = 0;

      modeStatus.enter();

      sleepWaitRelease = true;
    }
  }

  else {

    sleepStart = 0;
  }
}


/* ================================================================
 * BOOTSEL
 * ================================================================ */

void enterBootsel() {

  clearLeds();

  FastLED.setBrightness(0);
  FastLED.show();

  delay(40);

  reset_usb_boot(
    0,
    0
  );


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

  mainCursor = 0;

  modeStatus.enter();
}


/* ================================================================
 * UI HANDLER
 * ================================================================ */

void handleUI(
  Gesture g
) {

  if (
    g == G_NONE
  ) {

    return;
  }


  if (
    inAnyMenu()
  ) {

    touchMenu();
  }


  /*
   * VERY LONG HOLD
   *
   * Universal return to blue idle.
   */
  if (
    g == G_VERY_LONG
  ) {

    fill_solid(
      leds,
      NUM_LEDS,
      COLOR_HOT_PINK
    );

    FastLED.show();

    delay(120);

    returnToIdle();

    return;
  }


  /* ============================================================
   * FIREBALL
   * ============================================================ */

  if (
    uiState == UI_IN_GAME
  ) {

    /*
     * Long hold = leave game.
     *
     * The quick tap is handled by Fireball.
     */
    if (
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_GAMES;

      gamesCursor = 0;

      touchMenu();

      return;
    }


    appFireball.update(
      g
    );

    return;
  }


  /* ============================================================
   * NORMAL MODE
   * ============================================================ */

  if (
    uiState == UI_MODE
  ) {

    /*
     * HOLD opens home with the current item selected.
     */
    if (
      g == G_HOLD
    ) {

      mainCursor =
        currentMainMode;

      uiState =
        UI_MENU_MAIN;

      touchMenu();

      return;
    }


    /*
     * Long hold returns to blue idle.
     */
    if (
      g == G_LONG_HOLD
    ) {

      returnToIdle();

      return;
    }


    mainModes[
      currentMainMode
    ]->update(
      g
    );

    return;
  }


  /* ============================================================
   * HOME MENU
   * ============================================================ */

  if (
    uiState == UI_MENU_MAIN
  ) {

    /*
     * TAP = next.
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
     * DOUBLE = previous.
     */
    if (
      g == G_DOUBLE
    ) {

      if (
        mainCursor == 0
      ) {

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

        /*
         * STATUS
         */
        case 0:

          currentMainMode = 0;

          modeStatus.enter();

          uiState =
            UI_MODE;

          return;


        /*
         * SABER
         */
        case 1:

          currentMainMode = 1;

          modeSaber.enter();

          uiState =
            UI_MODE;

          return;


        /*
         * RAINBOW
         */
        case 2:

          currentMainMode = 2;

          modeRainbow.enter();

          uiState =
            UI_MODE;

          return;


        /*
         * GAMES
         */
        case 3:

          gamesCursor = 0;

          uiState =
            UI_MENU_GAMES;

          touchMenu();

          return;


        /*
         * SETTINGS
         */
        case 4:

          settingsCursor = 0;

          uiState =
            UI_MENU_SETTINGS;

          touchMenu();

          return;
      }
    }


    /*
     * LONG HOLD = default idle.
     */
    if (
      g == G_LONG_HOLD
    ) {

      returnToIdle();

      return;
    }
  }


  /* ============================================================
   * GAMES MENU
   * ============================================================ */

  if (
    uiState == UI_MENU_GAMES
  ) {

    /*
     * TAP = next.
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
     * DOUBLE = previous.
     */
    if (
      g == G_DOUBLE
    ) {

      if (
        gamesCursor == 0
      ) {

        gamesCursor = 1;
      }

      else {

        gamesCursor = 0;
      }

      return;
    }


    /*
     * HOLD = select.
     */
    if (
      g == G_HOLD
    ) {

      /*
       * BACK.
       */
      if (
        gamesCursor == 0
      ) {

        uiState =
          UI_MENU_MAIN;

        /*
         * Return cursor to Games.
         */
        mainCursor = 3;

        touchMenu();

        return;
      }


      /*
       * FIREBALL.
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
     * LONG HOLD = back to Home on Games.
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


  /* ============================================================
   * SETTINGS MENU
   * ============================================================ */

  if (
    uiState == UI_MENU_SETTINGS
  ) {

    /*
     * TAP = next.
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
     * DOUBLE = previous.
     */
    if (
      g == G_DOUBLE
    ) {

      if (
        settingsCursor == 0
      ) {

        settingsCursor =
          SETTINGS_COUNT - 1;
      }

      else {

        settingsCursor--;
      }

      return;
    }


    /*
     * HOLD = select.
     */
    if (
      g == G_HOLD
    ) {

      /*
       * BACK.
       */
      if (
        settingsCursor == 0
      ) {

        uiState =
          UI_MENU_MAIN;

        /*
         * Return cursor to Settings.
         */
        mainCursor = 4;

        touchMenu();

        return;
      }


      /*
       * BRIGHTNESS.
       */
      if (
        settingsCursor == 1
      ) {

        settingsReturnCursor = 1;


        /*
         * Find closest brightness level without min().
         */
        uint8_t best = 255;


        for (
          uint8_t i = 0;
          i < 10;
          i++
        ) {

          uint8_t difference =
            (uint8_t)abs(
              (int)brightVals[i] -
              (int)globalBrightness
            );


          if (
            difference < best
          ) {

            best =
              difference;

            brightLevel =
              i;
          }
        }


        uiState =
          UI_BRIGHTNESS;

        touchMenu();

        return;
      }


      /*
       * SPEED.
       */
      if (
        settingsCursor == 2
      ) {

        settingsReturnCursor = 2;


        uint8_t best = 255;


        for (
          uint8_t i = 0;
          i < 10;
          i++
        ) {

          uint8_t difference =
            (uint8_t)abs(
              (int)speedVals[i] -
              (int)globalSpeed
            );


          if (
            difference < best
          ) {

            best =
              difference;

            speedLevel =
              i;
          }
        }


        uiState =
          UI_SPEED;

        touchMenu();

        return;
      }


      /*
       * INFO.
       */
      if (
        settingsCursor == 3
      ) {

        settingsReturnCursor = 3;

        infoPage = 0;

        uiState =
          UI_INFO;

        touchMenu();

        return;
      }


      /*
       * BOOTSEL.
       */
      if (
        settingsCursor == 4
      ) {

        enterBootsel();

        return;
      }


      /*
       * SHUTDOWN.
       */
      if (
        settingsCursor == 5
      ) {

        enterShutdown();

        return;
      }
    }


    /*
     * LONG HOLD = Home on Settings.
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


  /* ============================================================
   * BRIGHTNESS
   * ============================================================ */

  if (
    uiState == UI_BRIGHTNESS
  ) {

    /*
     * TAP = next level.
     */
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

      touchMenu();

      return;
    }


    /*
     * DOUBLE = previous level.
     */
    if (
      g == G_DOUBLE
    ) {

      if (
        brightLevel == 0
      ) {

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

      touchMenu();

      return;
    }


    /*
     * HOLD/LONG = Settings.
     */
    if (
      g == G_HOLD ||
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_SETTINGS;

      settingsCursor =
        settingsReturnCursor;

      touchMenu();

      return;
    }
  }


  /* ============================================================
   * SPEED
   * ============================================================ */

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


      touchMenu();

      return;
    }


    if (
      g == G_DOUBLE
    ) {

      if (
        speedLevel == 0
      ) {

        speedLevel = 9;
      }

      else {

        speedLevel--;
      }


      globalSpeed =
        speedVals[
          speedLevel
        ];


      touchMenu();

      return;
    }


    if (
      g == G_HOLD ||
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_SETTINGS;

      settingsCursor =
        settingsReturnCursor;

      touchMenu();

      return;
    }
  }


  /* ============================================================
   * INFO
   * ============================================================ */

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


      touchMenu();

      return;
    }


    if (
      g == G_DOUBLE
    ) {

      if (
        infoPage == 0
      ) {

        infoPage = 2;
      }

      else {

        infoPage--;
      }


      touchMenu();

      return;
    }


    if (
      g == G_HOLD ||
      g == G_LONG_HOLD
    ) {

      uiState =
        UI_MENU_SETTINGS;

      settingsCursor =
        settingsReturnCursor;

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
   * Preserve known-good startup animation.
   */
  for (
    uint8_t i = 0;
    i < NUM_LEDS;
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
   * DEFAULT:
   * Blue bouncing ball.
   */
  currentMainMode = 0;

  mainCursor = 0;

  uiState =
    UI_MODE;

  modeStatus.enter();
}


/* ================================================================
 * LOOP
 * ================================================================ */

void loop() {

  static uint32_t lastFrame = 0;

  uint32_t now =
    millis();


  /*
   * SHUTDOWN
   */
  if (
    uiState == UI_SLEEP
  ) {

    updateShutdown();

    delay(5);

    return;
  }


  /*
   * ENTROPY
   */
  entropy.update();


  /*
   * MENU TIMEOUT
   */
  if (
    inAnyMenu() &&
    now - menuLastActivity >
      MENU_TIMEOUT_MS
  ) {

    returnToIdle();
  }


  /*
   * BUTTON
   */
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


  /*
   * FRAME
   */
  if (
    now - lastFrame >= FRAME_MS
  ) {

    lastFrame = now;

    renderUI();

    FastLED.show();
  }
}