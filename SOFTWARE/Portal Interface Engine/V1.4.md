#include <Arduino.h>
#include <FastLED.h>

/*
 * LAMOKA1 DEV BRAIN — Portal Interface Engine V1.4
 * -------------------------------------------------
 * FIRE: moving source + smooth heat diffusion
 * SABER: randomized color/pattern launches
 * FIREBALL: restored blast game behavior + binary score
 * SETTINGS: red shutdown, lowest startup brightness
 */

#define LED_PIN 11
#define NUM_LEDS 10
#define BUTTON_PIN 0
#define COLOR_ENTROPY_PIN 5
#define ADC0 26
#define ADC1 27
#define ADC2 28
#define ADC3 29

#define DEFAULT_BRIGHTNESS 18
#define DEFAULT_SPEED 100
#define FRAME_MS 16
#define MENU_TIMEOUT_MS 16000UL

CRGB leds[NUM_LEDS];

enum Gesture:uint8_t{G_NONE,G_TAP,G_DOUBLE,G_HOLD,G_LONG_HOLD,G_VERY_LONG};
enum UIState:uint8_t{UI_MODE,UI_MENU_MAIN,UI_MENU_APPS,UI_MENU_SETTINGS,
                     UI_BRIGHTNESS,UI_SPEED,UI_INFO,UI_IN_APP,UI_SLEEP};

uint8_t globalBrightness=DEFAULT_BRIGHTNESS,globalSpeed=DEFAULT_SPEED;

void applyBrightness(){uint8_t b=globalBrightness<18?18:globalBrightness;FastLED.setBrightness(b);}
void clearLeds(){fill_solid(leds,NUM_LEDS,CRGB::Black);}
CRGB blendColor(const CRGB&a,const CRGB&b,uint8_t t){return CRGB(lerp8by8(a.r,b.r,t),lerp8by8(a.g,b.g,t),lerp8by8(a.b,b.b,t));}

/* ENTROPY */
class EntropyService{
  uint32_t s=1;
  void mix(){
    uint32_t a=analogRead(ADC0),b=analogRead(ADC1),c=analogRead(ADC2),d=analogRead(ADC3);
    s^=(a<<16)^b^(c<<8)^d^micros()^(digitalRead(COLOR_ENTROPY_PIN)<<23);
    s^=s<<13;s^=s>>17;s^=s<<5;s+=0x9E3779B9UL;
  }
public:
  void begin(){
    pinMode(ADC0,INPUT);pinMode(ADC1,INPUT);pinMode(ADC2,INPUT);pinMode(ADC3,INPUT);
    pinMode(COLOR_ENTROPY_PIN,INPUT);s^=micros();for(int i=0;i<300;i++)mix();
  }
  void update(){mix();}
  uint32_t next(){mix();s^=s<<13;s^=s>>17;s^=s<<5;return s;}
  uint8_t next8(){return next()&255;}
  uint16_t range(uint16_t m){return m?next()%(m+1):0;}
  bool coin(){return next()&1;}
}entropy;

/* BUTTON */
class ButtonService{
  bool lastRaw=HIGH,wasPressed=false,pending=false,holdFired=false,longFired=false,veryLongFired=false;
  uint32_t pressStart=0,debounceStart=0,lastTap=0;
public:
  void begin(){pinMode(BUTTON_PIN,INPUT_PULLUP);lastRaw=digitalRead(BUTTON_PIN);}
  Gesture poll(bool inMenu){
    bool raw=digitalRead(BUTTON_PIN);uint32_t now=millis();
    if(raw!=lastRaw){debounceStart=now;lastRaw=raw;}
    if(now-debounceStart<28)return G_NONE;
    bool pressed=!raw;
    if(pressed&&!wasPressed){wasPressed=true;pressStart=now;holdFired=longFired=veryLongFired=false;return G_NONE;}
    if(pressed){
      uint32_t h=now-pressStart;
      if(h>=2800&&!veryLongFired){veryLongFired=true;return G_VERY_LONG;}
      if(h>=1200&&!longFired){longFired=true;return G_LONG_HOLD;}
      if(h>=550&&!holdFired){holdFired=true;return G_HOLD;}
    }
    if(!pressed&&wasPressed){
      wasPressed=false;uint32_t h=now-pressStart;
      if(!holdFired&&!longFired&&!veryLongFired&&h<400){
        if(inMenu)return G_TAP;
        if(lastTap&&now-lastTap<380){lastTap=0;pending=false;return G_DOUBLE;}
        lastTap=now;pending=true;
      }
    }
    if(!inMenu&&pending&&lastTap&&now-lastTap>=380){pending=false;return G_TAP;}
    return G_NONE;
  }
}button;

/* FIRE COLOR */
CRGB flameColor(uint8_t h){
  if(h<28)return blendColor(CRGB(8,0,0),CRGB(45,2,0),h*9);
  if(h<75)return blendColor(CRGB(45,2,0),CRGB(145,10,0),scale8(h-28,255));
  if(h<145)return blendColor(CRGB(145,10,0),CRGB(255,65,0),scale8(h-75,255));
  if(h<220)return blendColor(CRGB(255,65,0),CRGB(255,165,25),scale8(h-145,255));
  return blendColor(CRGB(255,165,25),CRGB(255,240,145),scale8(h-220,255));
}

/* BASE MODE */
class Mode{public:virtual~Mode(){}virtual void enter(){}virtual void update(Gesture){}virtual void render(){}};

/* STATUS */
class ModeStatus:public Mode{
  int8_t p=0,d=1;uint32_t last=0;
public:
  void enter()override{p=0;d=1;last=millis();}
  void render()override{
    clearLeds();uint16_t iv=28+8000/(globalSpeed+50);
    if(millis()-last>=iv){last=millis();p+=d;if(p>=9){p=9;d=-1;}if(p<=0){p=0;d=1;}}
    for(int t=0;t<3;t++){int q=p-d*t;if(q>=0&&q<10)leds[q]=CRGB(0,25+t*35,70+t*55);}
    leds[p]=CRGB(0,70,255);
  }
};

/* MOVING-SOURCE FIRE */
class ModeFire:public Mode{
  uint8_t heat[10]={0};
  int16_t src=4,target=4;
  uint16_t moveNoise=0;
  uint32_t lastMove=0;
  uint8_t flare=0;
  void newTarget(){target=1+entropy.range(7);}
public:
  void enter()override{
    memset(heat,0,sizeof(heat));src=4;target=4;
    moveNoise=entropy.next()&65535;lastMove=millis();flare=0;
    for(uint8_t i=0;i<10;i++)heat[i]=20+entropy.range(30);
  }
  void update(Gesture g)override{
    if(g==G_TAP)flare=150;
    if(g==G_DOUBLE){flare=230;newTarget();}
  }
  void render()override{
    uint32_t now=millis();

    /* The fire's SOURCE itself wanders left/right. */
    if(now-lastMove>240+entropy.range(260)){newTarget();lastMove=now;}
    if(src<target)src++;
    else if(src>target)src--;

    uint8_t wind=inoise8(now/95+moveNoise);
    int8_t drift=((int)wind-128)/32;

    /* slow cooling */
    for(uint8_t i=0;i<10;i++){
      uint8_t c=1+entropy.range(2)+i/6;
      heat[i]=(heat[i]>c)?heat[i]-c:0;
    }

    /* source injection */
    uint8_t fuel=105+entropy.range(45);
    uint8_t s=constrain(src+drift,0,9);
    heat[s]=qadd8(heat[s],fuel);
    if(s>0)heat[s-1]=qadd8(heat[s-1],fuel/3);
    if(s<9)heat[s+1]=qadd8(heat[s+1],fuel/2);

    /* convection/diffusion */
    uint8_t old[10];memcpy(old,heat,10);
    for(uint8_t i=1;i<9;i++){
      uint16_t avg=(uint16_t)old[i-1]+old[i]+old[i+1];
      heat[i]=lerp8by8(old[i],avg/3,145);
    }
    for(int i=9;i>=2;i--){
      uint16_t up=(uint16_t)heat[i-1]+heat[i-2];
      heat[i]=lerp8by8(heat[i],up/2,165);
    }

    /* occasional tongues that rise from the moving source */
    if(entropy.next8()>218){
      int q=s+(int)entropy.range(3)-1;
      q=constrain(q,1,7);
      uint8_t f=55+entropy.range(100);
      heat[q]=qadd8(heat[q],f);
      if(q<9)heat[q+1]=qadd8(heat[q+1],f/2);
    }

    if(flare){
      heat[s]=qadd8(heat[s],flare);
      if(s<9)heat[s+1]=qadd8(heat[s+1],flare/2);
      flare=flare>7?flare-7:0;
    }

    /* subtle organic modulation, never hard flicker */
    for(uint8_t i=0;i<10;i++){
      uint8_t n=inoise8(now/38+i*35);
      heat[i]=scale8(heat[i],220+(n>>5));
      uint8_t h=heat[i];
      if(i==0&&h<50)h=50;
      leds[i]=flameColor(h);
    }
  }
};

/* LIGHTSABER */
class ModeSaber:public Mode{
  uint8_t hue=0,sat=220,pattern=0,instability=30,power=0,clash=0;
  bool extending=true;uint32_t ignition=0;
  void launch(){hue=entropy.next8();sat=175+entropy.range(65);pattern=entropy.range(6);instability=20+entropy.range(80);power=0;extending=true;ignition=millis();}
public:
  void enter()override{launch();}
  void update(Gesture g)override{
    if(g==G_TAP){if(extending)extending=false;else launch();}
    if(g==G_DOUBLE)launch();
    if(g==G_HOLD)clash=16;
  }
  void render()override{
    clearLeds();uint8_t rate=7+globalSpeed/30;if(pattern==1)rate+=4;if(pattern==3&&rate>2)rate--;
    if(extending)power=qadd8(power,rate);else power=qsub8(power,10);
    uint8_t len=scale8(power,10);
    for(uint8_t i=0;i<len;i++){
      uint8_t b=qsub8(255,i*11);
      if(pattern==3&&entropy.next8()<instability)b=scale8(b,120);
      if(pattern==5&&entropy.next8()>220)b=255;
      if(pattern==4)b=scale8(b,beatsin8(12,145,255));
      if(pattern==2)b=scale8(b,beatsin8(9,185,255));
      if(pattern==1&&i==0)b=255;
      leds[i]=CHSV(hue,sat,b);
      if(i<2)leds[i]=blendColor(leds[i],CRGB::White,pattern==1?95:75);
    }
    if(extending&&millis()-ignition<170&&len){
      if(pattern==1)for(uint8_t i=0;i<10;i++)if(entropy.next8()>120)leds[i]=blendColor(leds[i],CRGB::White,120);
      else leds[0]=blendColor(leds[0],CRGB::White,180);
    }
    if(clash){for(uint8_t i=0;i<len;i++)if(entropy.next8()>90)leds[i]=blendColor(leds[i],CRGB::White,210);clash--;}
    if(len&&entropy.next8()>220){uint8_t p=entropy.next8()%len;leds[p]=blendColor(leds[p],CRGB::White,150);}
  }
};

/* OTHER MODES */
class ModeRainbow:public Mode{
  uint8_t hue=0,spd=3;bool rev=false;
public:
  void update(Gesture g)override{if(g==G_TAP)spd=spd%10+1;if(g==G_DOUBLE)rev=!rev;}
  void render()override{uint8_t s=spd*(globalSpeed+40)/75;if(!s)s=1;if(rev)hue-=s;else hue+=s;fill_rainbow(leds,10,hue,16);}
};

class ModePulse:public Mode{
  uint8_t hue=200,rate=4;
public:
  void update(Gesture g)override{if(g==G_TAP)hue+=30;if(g==G_DOUBLE)rate=rate%8+2;}
  void render()override{uint8_t r=rate*(globalSpeed+45)/95;if(r<2)r=2;fill_solid(leds,10,CHSV(hue,205,beatsin8(r,35,255)));}
};

class ModeEntropy:public Mode{
  uint16_t t=0;uint8_t spd=55,pal=0;
public:
  void update(Gesture g)override{if(g==G_TAP)spd=(spd+28)%150+25;if(g==G_DOUBLE)pal=(pal+1)%3;}
  void render()override{
    t+=spd*(globalSpeed+40)/85;
    for(uint8_t i=0;i<10;i++){uint8_t n=inoise8(t+i*42);
      if(pal==0)leds[i]=CHSV(n,215,210);
      else if(pal==1)leds[i]=CHSV(155+(n>>2),175,n);
      else leds[i]=CRGB(n,n/2,255-n);
    }
  }
};

class ModeBinary:public Mode{
  uint16_t v=0;uint8_t bit=0;
public:
  void enter()override{v=0;bit=0;}
  void update(Gesture g)override{if(g==G_TAP){v^=1<<bit;bit=(bit+1)%10;}if(g==G_DOUBLE)v=entropy.next()&0x3FF;if(g==G_HOLD)v=0;}
  void render()override{for(uint8_t i=0;i<10;i++)leds[i]=(v&(1<<i))?CRGB(0,180,255):CRGB(8,6,14);if((millis()/240)&1)leds[bit]=blendColor(leds[bit],CRGB::White,160);}
};

class ModeAmbient:public Mode{
public:
  void render()override{for(uint8_t i=0;i<10;i++){uint8_t n=inoise8(millis()/7+i*22);leds[i]=CHSV(145+(n>>3),170,35+(n>>2));}}
};

/* FIREBALL ENGINE */
class FireballFire{
  uint8_t heat[10];float pos=9;uint32_t seed=0;
public:
  void reset(){memset(heat,0,sizeof(heat));pos=9;seed=entropy.next();}
  void update(float p){
    pos=p;
    for(uint8_t i=0;i<10;i++){uint8_t c=5+entropy.range(4);heat[i]=heat[i]>c?heat[i]-c:0;}
    for(int i=9;i>=2;i--){uint16_t a=(uint16_t)heat[i-1]+heat[i-2];heat[i]=lerp8by8(heat[i],a/2,180);}
    int h=(int)(pos+0.5f);
    if(h>=0&&h<10){
      heat[h]=qadd8(heat[h],255);
      if(h>0)heat[h-1]=qadd8(heat[h-1],110);
      if(h<9)heat[h+1]=qadd8(heat[h+1],160);
    }
    uint8_t old[10];memcpy(old,heat,10);
    for(uint8_t i=1;i<9;i++){uint16_t a=(uint16_t)old[i-1]+old[i]+old[i+1];heat[i]=qadd8(heat[i],scale8(a/3,55));}
    seed+=23+entropy.range(40);
  }
  void render(){
    for(uint8_t i=0;i<10;i++){uint8_t h=heat[i];if(!h){leds[i]=CRGB::Black;continue;}leds[i]=flameColor(h);}
    int h=(int)(pos+0.5f);if(h>=0&&h<10)leds[h]=blendColor(leds[h],CRGB(255,235,100),230);
  }
};

/* FIREBALL GAME */
class AppFireball{
  enum{RUN,BETWEEN,DEAD}state=RUN;
  uint8_t score=0,lives=3;uint16_t speed=160;float pos=9;uint32_t last=0;
  FireballFire fire;
public:
  void enter(){score=0;lives=3;speed=160;pos=9;state=RUN;last=millis();fire.reset();}
  void update(Gesture g){
    if(state==DEAD){if(g==G_TAP)enter();return;}
    if(state==BETWEEN){if(g==G_TAP){state=RUN;pos=9;last=millis();fire.reset();}return;}
    if(state==RUN&&g==G_TAP){
      if(pos<=1.6f){score++;state=BETWEEN;speed=speed*82/100;if(speed<38)speed=38;}
      else{if(--lives==0)state=DEAD;else state=BETWEEN;}
    }
  }
  void render(){
    clearLeds();uint32_t now=millis();
    if(state==RUN){
      leds[0]=CRGB(0,110,25);
      pos-=(float)(now-last)/speed;last=now;
      fire.update(pos);fire.render();
      if(pos>0.8f)leds[0]=blendColor(CRGB(0,110,25),leds[0],100);
      if(pos<-0.8f){if(--lives==0)state=DEAD;else state=BETWEEN;return;}
      return;
    }
    if(state==BETWEEN){
      /* ORIGINAL BINARY SCORE DISPLAY — nothing over it */
      for(uint8_t i=0;i<10;i++)leds[i]=(score&(1<<i))?CRGB::White:CRGB::Black;
      return;
    }
    fill_solid(leds,10,CRGB(beatsin8(4,125,255),0,0));
  }
}appFireball;

/* MODES */
ModeStatus modeStatus;
ModeFire modeFire;
ModeSaber modeSaber;
ModeRainbow modeRainbow;
ModePulse modePulse;
ModeEntropy modeEntropy;
ModeBinary modeBinary;
ModeAmbient modeAmbient;

Mode* mainModes[]={
  &modeStatus,&modeFire,&modeSaber,&modeRainbow,
  &modePulse,&modeEntropy,&modeBinary,&modeAmbient
};

const uint8_t MAIN_MODE_COUNT=8;

/* UI */
UIState uiState=UI_MODE;
uint8_t mainCursor=0,appsCursor=0,settingsCursor=0,currentMainMode=0;
uint32_t menuLastActivity=0;
bool menuInit=false;

void touchMenu(){menuLastActivity=millis();}
bool inAnyMenu(){
  return uiState==UI_MENU_MAIN||uiState==UI_MENU_APPS||
         uiState==UI_MENU_SETTINGS||uiState==UI_BRIGHTNESS||
         uiState==UI_SPEED||uiState==UI_INFO;
}

/* MAIN MENU */
void renderMainMenu(){
  for(uint8_t i=0;i<10;i++)leds[i]=CRGB(0,18,4);
  leds[8]=CRGB(255,90,0);leds[9]=CRGB(0,50,100);
  CRGB c=CRGB(255,40,160);c.nscale8(beatsin8(7,210,255));leds[mainCursor]=c;
}

/* APPS */
void renderAppsMenu(){
  clearLeds();leds[0]=CRGB(255,100,0);leds[1]=CRGB(255,0,0);
  CRGB c=CRGB(255,255,220);c.nscale8(beatsin8(9,200,255));leds[appsCursor]=c;
}

/* SETTINGS: 0 BACK,1 BRIGHTNESS,2 SPEED,3 INFO,4 SHUTDOWN */
void renderSettingsMenu(){
  clearLeds();
  leds[0]=CRGB(0,55,100);
  leds[1]=leds[2]=leds[3]=CRGB(0,25,6);
  leds[4]=CRGB(255,0,0); /* RED SHUTDOWN */
  CRGB c=CRGB(255,40,160);
  if(settingsCursor==0)leds[0]=blendColor(leds[0],c,200);
  else{leds[settingsCursor]=c;leds[settingsCursor].nscale8(beatsin8(7,200,255));}
}

/* BRIGHTNESS */
const uint8_t brightVals[10]={18,24,32,42,55,70,85,100,115,130};
uint8_t brightLevel=0;
void renderBrightness(){
  for(uint8_t i=0;i<10;i++)leds[i]=(i<=brightLevel)?CRGB(255,50,160):CRGB(0,20,5);
}

/* SPEED */
const uint8_t speedVals[10]={40,55,70,85,100,120,145,170,200,240};
uint8_t speedLevel=4;
void renderSpeed(){
  for(uint8_t i=0;i<10;i++)leds[i]=(i<=speedLevel)?CRGB(255,50,160):CRGB(0,20,5);
}

/* INFO */
uint8_t infoPage=0;
void renderInfo(){
  clearLeds();
  if(infoPage==0){fill_solid(leds,10,CRGB(0,16,20));leds[0]=CRGB(0,110,255);}
  else if(infoPage==1){uint8_t n=(millis()/1000)%11;for(uint8_t i=0;i<10;i++)leds[i]=(i<n)?CRGB(0,35,12):CRGB::Black;}
  else fill_solid(leds,10,CRGB(25,6,0));
}

/* SHUTDOWN */
bool sleepWaitRelease=true;uint32_t sleepStart=0;
void enterShutdown(){uiState=UI_SLEEP;sleepWaitRelease=true;sleepStart=0;clearLeds();FastLED.setBrightness(0);FastLED.show();}
void updateShutdown(){
  clearLeds();FastLED.setBrightness(0);FastLED.show();
  if(sleepWaitRelease){if(digitalRead(BUTTON_PIN)==HIGH)sleepWaitRelease=false;return;}
  if(digitalRead(BUTTON_PIN)==LOW){
    if(!sleepStart)sleepStart=millis();
    if(millis()-sleepStart>=1500){
      sleepStart=0;applyBrightness();uiState=UI_MODE;currentMainMode=0;mainModes[0]->enter();sleepWaitRelease=true;
    }
  }else sleepStart=0;
}

/* UI HANDLER */
void handleUI(Gesture g){
  if(inAnyMenu())touchMenu();

  if(g==G_VERY_LONG){
    fill_solid(leds,10,CRGB::Red);FastLED.show();delay(260);
    uiState=UI_MODE;currentMainMode=0;mainModes[0]->enter();return;
  }

  if(uiState==UI_IN_APP){
    if(g==G_LONG_HOLD){uiState=UI_MENU_APPS;touchMenu();return;}
    appFireball.update(g);return;
  }

  if(uiState==UI_MODE){
    if(g==G_HOLD){
      if(!menuInit){mainCursor=currentMainMode;menuInit=true;}
      uiState=UI_MENU_MAIN;touchMenu();return;
    }
    if(g==G_LONG_HOLD){currentMainMode=0;mainModes[0]->enter();return;}
    mainModes[currentMainMode]->update(g);return;
  }

  if(uiState==UI_MENU_MAIN){
    if(g==G_TAP){mainCursor=(mainCursor+1)%10;return;}
    if(g==G_HOLD){
      if(mainCursor==8){appsCursor=0;uiState=UI_MENU_APPS;touchMenu();}
      else if(mainCursor==9){settingsCursor=0;uiState=UI_MENU_SETTINGS;touchMenu();}
      else if(mainCursor<MAIN_MODE_COUNT){currentMainMode=mainCursor;mainModes[currentMainMode]->enter();uiState=UI_MODE;}
      return;
    }
    if(g==G_LONG_HOLD){uiState=UI_MODE;return;}
  }

  if(uiState==UI_MENU_APPS){
    if(g==G_TAP){appsCursor=(appsCursor+1)%2;return;}
    if(g==G_HOLD){
      if(appsCursor==0){uiState=UI_MENU_MAIN;touchMenu();}
      else{appFireball.enter();uiState=UI_IN_APP;touchMenu();}
      return;
    }
    if(g==G_LONG_HOLD){uiState=UI_MENU_MAIN;touchMenu();return;}
  }

  if(uiState==UI_MENU_SETTINGS){
    if(g==G_TAP){settingsCursor=(settingsCursor+1)%5;return;}
    if(g==G_HOLD){
      switch(settingsCursor){
        case 0:uiState=UI_MENU_MAIN;break;
        case 1:{
          uint8_t best=255;
          for(uint8_t i=0;i<10;i++){uint8_t d=abs((int)brightVals[i]-(int)globalBrightness);if(d<best){best=d;brightLevel=i;}}
          uiState=UI_BRIGHTNESS;break;
        }
        case 2:uiState=UI_SPEED;break;
        case 3:infoPage=0;uiState=UI_INFO;break;
        case 4:enterShutdown();return;
      }
      touchMenu();return;
    }
    if(g==G_LONG_HOLD){uiState=UI_MODE;currentMainMode=0;mainModes[0]->enter();return;}
  }

  if(uiState==UI_BRIGHTNESS){
    if(g==G_TAP){brightLevel=(brightLevel+1)%10;globalBrightness=brightVals[brightLevel];applyBrightness();return;}
    if(g==G_HOLD||g==G_LONG_HOLD){uiState=UI_MENU_SETTINGS;touchMenu();return;}
  }

  if(uiState==UI_SPEED){
    if(g==G_TAP){speedLevel=(speedLevel+1)%10;globalSpeed=speedVals[speedLevel];return;}
    if(g==G_HOLD||g==G_LONG_HOLD){uiState=UI_MENU_SETTINGS;touchMenu();return;}
  }

  if(uiState==UI_INFO){
    if(g==G_TAP){infoPage=(infoPage+1)%3;return;}
    if(g==G_HOLD||g==G_LONG_HOLD){uiState=UI_MENU_SETTINGS;touchMenu();return;}
  }
}

/* RENDER */
void renderUI(){
  switch(uiState){
    case UI_MODE:mainModes[currentMainMode]->render();break;
    case UI_MENU_MAIN:renderMainMenu();break;
    case UI_MENU_APPS:renderAppsMenu();break;
    case UI_MENU_SETTINGS:renderSettingsMenu();break;
    case UI_BRIGHTNESS:renderBrightness();break;
    case UI_SPEED:renderSpeed();break;
    case UI_INFO:renderInfo();break;
    case UI_IN_APP:appFireball.render();break;
    default:clearLeds();break;
  }
}

/* SETUP */
void setup(){
  FastLED.addLeds<WS2812B,LED_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setDither(0);
  applyBrightness();
  FastLED.clear(true);
  entropy.begin();
  button.begin();

  for(uint8_t i=0;i<10;i++){
    leds[i]=CHSV(i*25,230,210);
    FastLED.show();
    delay(18);
  }
  delay(70);
  clearLeds();FastLED.show();

  currentMainMode=0;
  mainModes[0]->enter();
}

/* LOOP */
void loop(){
  static uint32_t lastFrame=0;
  uint32_t now=millis();

  if(uiState==UI_SLEEP){
    updateShutdown();
    delay(5);
    return;
  }

  entropy.update();

  if(inAnyMenu()&&now-menuLastActivity>MENU_TIMEOUT_MS){
    uiState=UI_MODE;currentMainMode=0;mainModes[0]->enter();
  }

  Gesture g=button.poll(inAnyMenu()||uiState==UI_IN_APP);
  if(g!=G_NONE)handleUI(g);

  if(now-lastFrame>=FRAME_MS){
    lastFrame=now;
    renderUI();
    FastLED.show();
  }
}