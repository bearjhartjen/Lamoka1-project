/*
 * ================================================================
 * LAMOKA1 DEV BRAIN — Portal Interface Engine V1.1 
 * ================================================================
 */

#include <Arduino.h>
#include <FastLED.h>

#define LED_PIN            11
#define NUM_LEDS           10
#define BUTTON_PIN         0
#define COLOR_ENTROPY_PIN  5
#define ADC0 26
#define ADC1 27
#define ADC2 28
#define ADC3 29

#define DEFAULT_BRIGHTNESS 36
#define DEFAULT_SPEED      100
#define FRAME_MS           16
#define MENU_TIMEOUT_MS    16000UL

CRGB leds[NUM_LEDS];

enum Gesture : uint8_t { G_NONE=0, G_TAP, G_DOUBLE, G_HOLD, G_LONG_HOLD, G_VERY_LONG };

enum UIState : uint8_t {
  UI_MODE=0, UI_MENU_MAIN, UI_MENU_APPS, UI_MENU_SETTINGS,
  UI_BRIGHTNESS, UI_SPEED, UI_INFO, UI_IN_APP
};

uint8_t globalBrightness = DEFAULT_BRIGHTNESS;
uint8_t globalSpeed = DEFAULT_SPEED;

void applyBrightness() {
  uint8_t b = globalBrightness;
  if (b < 18) b = 18;          // never too dark for menus
  FastLED.setBrightness(b);
}

void clearLeds() { fill_solid(leds, NUM_LEDS, CRGB::Black); }

CRGB blendColor(const CRGB& a, const CRGB& b, uint8_t t) {
  return CRGB(lerp8by8(a.r,b.r,t), lerp8by8(a.g,b.g,t), lerp8by8(a.b,b.b,t));
}

/* ================================================================
 * ENTROPY + BUTTON
 * ================================================================ */
class EntropyService {
public:
  void begin() {
    pinMode(ADC0,INPUT); pinMode(ADC1,INPUT); pinMode(ADC2,INPUT); pinMode(ADC3,INPUT);
    pinMode(COLOR_ENTROPY_PIN,INPUT);
    state = 0xA341316CUL ^ micros();
    for(int i=0;i<400;i++) mix();
  }
  void update() { mix(); }
  uint32_t next() {
    mix(); state ^= state<<13; state ^= state>>17; state ^= state<<5; return state;
  }
  uint8_t next8() { return next() & 0xFF; }
  uint16_t range(uint16_t m) { return m ? next() % (m+1) : 0; }
  bool coin() { return next() & 1; }
private:
  uint32_t state = 1;
  void mix() {
    uint32_t a=analogRead(ADC0),b=analogRead(ADC1),c=analogRead(ADC2),d=analogRead(ADC3);
    uint32_t t=micros(), p=digitalRead(COLOR_ENTROPY_PIN);
    state ^= (a<<16)^b; state ^= (c<<8)^d; state ^= t; state ^= p<<23;
    state ^= state<<13; state ^= state>>17; state ^= state<<5; state += 0x9E3779B9UL;
  }
};
EntropyService entropy;

class ButtonService {
public:
  void begin() { pinMode(BUTTON_PIN, INPUT_PULLUP); lastRaw = digitalRead(BUTTON_PIN); }
  Gesture poll(bool inMenu) {
    bool raw = digitalRead(BUTTON_PIN);
    uint32_t now = millis();
    if (raw != lastRaw) { debounceStart = now; lastRaw = raw; }
    if (now - debounceStart < 28) return G_NONE;
    bool pressed = !raw;

    if (pressed && !wasPressed) {
      wasPressed = true; pressStart = now;
      holdFired = longFired = veryLongFired = false;
      return G_NONE;
    }
    if (pressed) {
      uint32_t h = now - pressStart;
      if (h >= 2800 && !veryLongFired) { veryLongFired=true; return G_VERY_LONG; }
      if (h >= 1200 && !longFired)     { longFired=true; return G_LONG_HOLD; }
      if (h >= 550  && !holdFired)     { holdFired=true; return G_HOLD; }
    }
    if (!pressed && wasPressed) {
      wasPressed = false;
      uint32_t h = now - pressStart;
      if (!holdFired && !longFired && !veryLongFired && h < 400) {
        if (inMenu) return G_TAP;
        if (lastTap && now - lastTap < 380) { lastTap=0; pending=false; return G_DOUBLE; }
        lastTap = now; pending = true;
      }
    }
    if (!inMenu && pending && lastTap && now - lastTap >= 380) {
      pending = false; return G_TAP;
    }
    return G_NONE;
  }
private:
  bool lastRaw=HIGH, wasPressed=false, pending=false;
  bool holdFired=false, longFired=false, veryLongFired=false;
  uint32_t pressStart=0, debounceStart=0, lastTap=0;
};
ButtonService button;

/* ================================================================
 * NORMAL MODES
 * ================================================================ */
class Mode {
public:
  virtual ~Mode() {}
  virtual void enter() {}
  virtual void update(Gesture) {}
  virtual void render() {}
};

class ModeStatus : public Mode {
public:
  void enter() override { pos=0; dir=1; last=millis(); }
  void render() override {
    clearLeds();
    uint16_t iv = 28 + 80*100/(globalSpeed+50);
    if (millis()-last >= iv) {
      last=millis(); pos+=dir;
      if (pos>=9){pos=9;dir=-1;} if (pos<=0){pos=0;dir=1;}
    }
    for(int t=0;t<3;t++){
      int p=pos-dir*t;
      if(p>=0&&p<10) leds[p]=CRGB(0,25+t*35,70+t*55);
    }
    leds[pos]=CRGB(0,70,255);
  }
private:
  int8_t pos=0,dir=1; uint32_t last=0;
};

class ModeFire : public Mode {
public:
  void enter() override { noise=entropy.next(); }
  void update(Gesture g) override {
    if(g==G_TAP) intensity=(intensity+45)%210+40;
    if(g==G_DOUBLE) cool=!cool;
  }
  void render() override {
    noise += (320+intensity/3)*(globalSpeed+45)/85;
    for(uint8_t i=0;i<10;i++){
      uint8_t heat=40+scale8(inoise8(noise+i*40),185);
      if(cool) heat=scale8(heat,155);
      CRGB c;
      if(heat<65) c=blendColor(CRGB(40,0,0),CRGB(200,15,0),heat*4);
      else if(heat<140) c=blendColor(CRGB(200,15,0),CRGB(255,90,0),(heat-65)*3);
      else c=blendColor(CRGB(255,90,0),CRGB(255,190,50),(heat-140)*3);
      leds[i]=c;
    }
  }
private:
  uint32_t noise=0; uint8_t intensity=110; bool cool=false;
};

class ModeSaber : public Mode {
public:
  void enter() override { hue=entropy.next8(); power=0; ext=true; }
  void update(Gesture g) override {
    if(g==G_TAP) ext=!ext;
    if(g==G_DOUBLE) hue=entropy.next8();
    if(g==G_HOLD) clash=14;
  }
  void render() override {
    if(ext&&power<255) power=qadd8(power,10);
    if(!ext&&power>0) power=qsub8(power,8);
    uint8_t len=scale8(power,10);
    for(uint8_t i=0;i<10;i++){
      if(i<len){
        uint8_t b=255-i*17; if(clash) b=255;
        leds[i]=CHSV(hue,235,b);
        if(i<2) leds[i]=blendColor(leds[i],CRGB::White,75);
      } else leds[i]=CRGB::Black;
    }
    if(clash) clash--;
  }
private:
  uint8_t hue=0,power=0,clash=0; bool ext=true;
};

class ModeRainbow : public Mode {
public:
  void update(Gesture g) override { if(g==G_TAP) spd=(spd%10)+1; if(g==G_DOUBLE) rev=!rev; }
  void render() override {
    uint8_t step=spd*(globalSpeed+40)/75; if(!step) step=1;
    if(rev) hue-=step; else hue+=step;
    fill_rainbow(leds,10,hue,16);
  }
private: uint8_t hue=0,spd=3; bool rev=false;
};

class ModePulse : public Mode {
public:
  void update(Gesture g) override { if(g==G_TAP) hue+=30; if(g==G_DOUBLE) rate=(rate%8)+2; }
  void render() override {
    uint8_t r=rate*(globalSpeed+45)/95; if(r<2) r=2;
    fill_solid(leds,10,CHSV(hue,205,beatsin8(r,35,255)));
  }
private: uint8_t hue=200,rate=4;
};

class ModeEntropy : public Mode {
public:
  void update(Gesture g) override { if(g==G_TAP) spd=(spd+28)%150+25; if(g==G_DOUBLE) pal=(pal+1)%3; }
  void render() override {
    t+=spd*(globalSpeed+40)/85;
    for(uint8_t i=0;i<10;i++){
      uint8_t n=inoise8(t+i*42);
      if(pal==0) leds[i]=CHSV(n,215,210);
      else if(pal==1) leds[i]=CHSV(155+(n>>2),175,n);
      else leds[i]=CRGB(n,n/2,255-n);
    }
  }
private: uint16_t t=0; uint8_t spd=55,pal=0;
};

class ModeBinary : public Mode {
public:
  void enter() override { val=0; bit=0; }
  void update(Gesture g) override {
    if(g==G_TAP){ val^=(1<<bit); bit=(bit+1)%10; }
    if(g==G_DOUBLE) val=entropy.next()&0x3FF;
    if(g==G_HOLD) val=0;
  }
  void render() override {
    for(uint8_t i=0;i<10;i++) leds[i]=(val&(1<<i))?CRGB(0,180,255):CRGB(8,6,14);
    if((millis()/240)&1) leds[bit]=blendColor(leds[bit],CRGB::White,160);
  }
private: uint16_t val=0; uint8_t bit=0;
};

class ModeAmbient : public Mode {
public:
  void render() override {
    for(uint8_t i=0;i<10;i++){
      uint8_t n=inoise8(millis()/7+i*22);
      leds[i]=CHSV(145+(n>>3),170,35+(n>>2));
    }
  }
};

/* ================================================================
 * FIREBALL APPLICATION (Standalone App with Lives & Binary Score)
 * ================================================================ */
class AppFireball {
public:
  void enter() {
    score=0; lives=3; speed=160; pos=9.0f; state=RUN; last=millis();
  }
  void update(Gesture g) {
    if(state==DEAD){ 
      if(g==G_TAP) enter(); 
      return; 
    }
    if(state==BETWEEN){
      if(g==G_TAP){ state=RUN; pos=9.0f; last=millis(); }
      return;
    }
    if(state==RUN && g==G_TAP){
      if(pos <= 1.6f){               // successful defuse window
        score++;
        state=BETWEEN;
        speed = speed * 82 / 100;
        if(speed < 38) speed=38;
      } else {
        lives--;
        if(lives == 0){
          state=DEAD;
        } else {
          state=BETWEEN;
        }
      }
    }
  }
  void render() {
    clearLeds();
    uint32_t now=millis();

    if(state==RUN){
      // permanent green safe zone on D0
      leds[0] = CRGB(0, 110, 25);

      float step = (float)(now-last) / speed;
      last=now;
      pos -= step;
      if(pos < -0.5f){ 
        lives--;
        if(lives == 0){
          state=DEAD;
        } else {
          state=BETWEEN;
        }
        return; 
      }

      int head = (int)pos;
      for(int t=0;t<5;t++){
        int p = head + t;
        if(p<0 || p>9) continue;
        uint8_t heat = 255 - t*48;
        if(t==0){                     
          leds[p] = CRGB(255, 220, 60);
        } else if(t==1){
          leds[p] = CRGB(255, 120, 10);
        } else {
          leds[p] = CRGB(heat, heat/4, 0);
        }
        if(entropy.next8() > 200) leds[p].nscale8(180);
      }
    }
    else if(state==BETWEEN){
      // Display score in binary using white dots between launches
      for(uint8_t i=0; i<10; i++){
        if(score & (1 << i)){
          leds[i] = CRGB::White;
        } else {
          leds[i] = CRGB::Black;
        }
      }
    }
    else { // DEAD - Red all around
      fill_solid(leds, 10, CRGB::Red);
    }
  }
private:
  enum { RUN, BETWEEN, DEAD };
  uint8_t state, score, lives;
  float pos;
  uint16_t speed;
  uint32_t last;
};

AppFireball appFireball;

ModeStatus modeStatus; 
ModeFire modeFire; 
ModeSaber modeSaber;
ModeRainbow modeRainbow; 
ModePulse modePulse; 
ModeEntropy modeEntropy;
ModeBinary modeBinary; 
ModeAmbient modeAmbient;

Mode* mainModes[] = {
  &modeStatus, &modeFire, &modeSaber, &modeRainbow,
  &modePulse, &modeEntropy, &modeBinary, &modeAmbient
};

/* ================================================================
 * UI
 * ================================================================ */
UIState uiState = UI_MODE;
uint8_t mainCursor=0, appsCursor=0, settingsCursor=0;
uint8_t currentMainMode=0;
uint32_t menuLastActivity=0;
bool menuInit=false;

void touchMenu(){ menuLastActivity=millis(); }
bool inAnyMenu(){
  return uiState==UI_MENU_MAIN||uiState==UI_MENU_APPS||uiState==UI_MENU_SETTINGS||
         uiState==UI_BRIGHTNESS||uiState==UI_SPEED||uiState==UI_INFO;
}

/* ---- MAIN MENU ---- */
void renderMainMenu() {
  for(uint8_t i=0;i<10;i++) leds[i]=CRGB(0,18,4);
  leds[8]=CRGB(255,90,0);          // bright ORANGE apps portal
  leds[9]=CRGB(0,50,100);          // cyan settings

  CRGB pink=CRGB(255,40,160);
  pink.nscale8(beatsin8(7,210,255));
  leds[mainCursor]=pink;
}

/* ---- APPS MENU ---- */
void renderAppsMenu() {
  for(uint8_t i=0;i<10;i++) leds[i] = CRGB::Black;
  leds[0] = CRGB(255, 100, 0);     // Back portal
  leds[1] = CRGB(255, 0, 0);       // Fireball App icon (Red)

  CRGB cursor = CRGB(255, 255, 220);
  cursor.nscale8(beatsin8(9, 200, 255));
  leds[appsCursor] = cursor;
}

/* ---- SETTINGS ---- */
void renderSettingsMenu() {
  clearLeds();
  leds[0]=CRGB(0,55,100);
  for(uint8_t i=1;i<4;i++) leds[i]=CRGB(0,25,6);
  CRGB pink=CRGB(255,40,160);
  if(settingsCursor==0) leds[0]=blendColor(leds[0],pink,200);
  else {
    leds[settingsCursor]=pink;
    leds[settingsCursor].nscale8(beatsin8(7,200,255));
  }
}

const uint8_t brightVals[10]={18,24,32,42,55,70,85,100,115,130};
uint8_t brightLevel=3;
void renderBrightness(){
  for(uint8_t i=0;i<10;i++)
    leds[i]=(i<=brightLevel)?CRGB(255,50,160):CRGB(0,20,5);
}
const uint8_t speedVals[10]={40,55,70,85,100,120,145,170,200,240};
uint8_t speedLevel=4;
void renderSpeed(){
  for(uint8_t i=0;i<10;i++)
    leds[i]=(i<=speedLevel)?CRGB(255,50,160):CRGB(0,20,5);
}
uint8_t infoPage=0;
void renderInfo(){
  clearLeds();
  if(infoPage==0){fill_solid(leds,10,CRGB(0,16,20));leds[0]=CRGB(0,110,255);}
  else if(infoPage==1){
    uint8_t lit=(millis()/1000)%11;
    for(uint8_t i=0;i<10;i++) leds[i]=(i<lit)?CRGB(0,35,12):CRGB::Black;
  } else fill_solid(leds,10,CRGB(25,6,0));
}

/* ================================================================
 * HANDLER
 * ================================================================ */
void handleUI(Gesture g){
  if(inAnyMenu()) touchMenu();

  if(g==G_VERY_LONG){
    fill_solid(leds,10,CRGB::Red); FastLED.show(); delay(260);
    uiState=UI_MODE; currentMainMode=0; mainModes[0]->enter(); return;
  }

  if(uiState==UI_IN_APP){
    if(g==G_LONG_HOLD){ uiState=UI_MENU_APPS; touchMenu(); return; }
    appFireball.update(g);
    return;
  }

  if(uiState==UI_MODE){
    if(g==G_HOLD){
      if(!menuInit){mainCursor=currentMainMode;menuInit=true;}
      uiState=UI_MENU_MAIN; touchMenu(); return;
    }
    if(g==G_LONG_HOLD){ currentMainMode=0; mainModes[0]->enter(); return; }
    mainModes[currentMainMode]->update(g); return;
  }

  if(uiState==UI_MENU_MAIN){
    if(g==G_TAP){ mainCursor=(mainCursor+1)%10; return; }
    if(g==G_HOLD){
      if(mainCursor==8){ appsCursor=0; uiState=UI_MENU_APPS; touchMenu(); }
      else if(mainCursor==9){ settingsCursor=0; uiState=UI_MENU_SETTINGS; touchMenu(); }
      else {
        currentMainMode=mainCursor; mainModes[currentMainMode]->enter(); uiState=UI_MODE;
      }
      return;
    }
    if(g==G_LONG_HOLD){ uiState=UI_MODE; return; }
    return;
  }

  if(uiState==UI_MENU_APPS){
    if(g==G_TAP){ appsCursor=(appsCursor+1)%2; return; }
    if(g==G_HOLD){
      if(appsCursor==0){
        uiState=UI_MENU_MAIN; touchMenu();
      } else if(appsCursor==1){
        appFireball.enter();
        uiState=UI_IN_APP;
        touchMenu();
      }
      return;
    }
    if(g==G_LONG_HOLD){ uiState=UI_MENU_MAIN; touchMenu(); return; }
    return;
  }

  if(uiState==UI_MENU_SETTINGS){
    if(g==G_TAP){ settingsCursor=(settingsCursor+1)%4; return; }
    if(g==G_HOLD){
      if(settingsCursor==0){ uiState=UI_MENU_MAIN; touchMenu(); }
      else if(settingsCursor==1){
        uint8_t best=255; brightLevel=0;
        for(uint8_t i=0;i<10;i++){
          uint8_t d=abs((int)brightVals[i]-(int)globalBrightness);
          if(d<best){best=d;brightLevel=i;}
        }
        uiState=UI_BRIGHTNESS;
      } else if(settingsCursor==2) uiState=UI_SPEED;
      else { infoPage=0; uiState=UI_INFO; }
      touchMenu(); return;
    }
    if(g==G_LONG_HOLD){ uiState=UI_MODE; currentMainMode=0; mainModes[0]->enter(); return; }
    return;
  }

  if(uiState==UI_BRIGHTNESS){
    if(g==G_TAP){
      brightLevel=(brightLevel+1)%10;
      globalBrightness=brightVals[brightLevel]; applyBrightness(); return;
    }
    if(g==G_HOLD||g==G_LONG_HOLD){ uiState=UI_MENU_SETTINGS; touchMenu(); return; }
    return;
  }
  if(uiState==UI_SPEED){
    if(g==G_TAP){
      speedLevel=(speedLevel+1)%10; globalSpeed=speedVals[speedLevel]; return;
    }
    if(g==G_HOLD||g==G_LONG_HOLD){ uiState=UI_MENU_SETTINGS; touchMenu(); return; }
    return;
  }
  if(uiState==UI_INFO){
    if(g==G_TAP){ infoPage=(infoPage+1)%3; return; }
    if(g==G_HOLD||g==G_LONG_HOLD){ uiState=UI_MENU_SETTINGS; touchMenu(); return; }
    return;
  }
}

void renderUI(){
  switch(uiState){
    case UI_MODE: mainModes[currentMainMode]->render(); break;
    case UI_MENU_MAIN: renderMainMenu(); break;
    case UI_MENU_APPS: renderAppsMenu(); break;
    case UI_MENU_SETTINGS: renderSettingsMenu(); break;
    case UI_BRIGHTNESS: renderBrightness(); break;
    case UI_SPEED: renderSpeed(); break;
    case UI_INFO: renderInfo(); break;
    case UI_IN_APP: appFireball.render(); break;
    default: clearLeds(); break;
  }
}

void setup(){
  FastLED.addLeds<WS2812B,LED_PIN,GRB>(leds,NUM_LEDS);
  applyBrightness(); FastLED.setDither(0); FastLED.clear(true);
  entropy.begin(); button.begin();

  for(uint8_t i=0;i<10;i++){ leds[i]=CHSV(i*25,230,210); FastLED.show(); delay(18); }
  delay(70); clearLeds(); FastLED.show();
  currentMainMode=0; mainModes[0]->enter();
}

void loop(){
  static uint32_t lastFrame=0;
  uint32_t now=millis();
  entropy.update();

  if(inAnyMenu() && now-menuLastActivity>MENU_TIMEOUT_MS){
    uiState=UI_MODE; currentMainMode=0; mainModes[0]->enter();
  }

  Gesture g=button.poll(inAnyMenu()||uiState==UI_IN_APP);
  if(g!=G_NONE) handleUI(g);

  if(now-lastFrame>=FRAME_MS){
    lastFrame=now; renderUI(); FastLED.show();
  }
}