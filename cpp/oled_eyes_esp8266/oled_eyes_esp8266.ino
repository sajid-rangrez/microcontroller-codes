/*
 * ╔══════════════════════════════════════════════════════════════╗
 *  Robo Face  –  ESP8266 + 0.96" I2C OLED  (128×64 SSD1306)
 *  Style: filled rounded-rect eyes, cutout shapes per emotion
 * ╠══════════════════════════════════════════════════════════════╣
 *  Wiring:
 *    OLED VCC → 3.3V        Touch VCC → 3.3V
 *    OLED GND → GND         Touch GND → GND
 *    OLED SDA → D2 (GPIO4)  Touch SIG → D5 (GPIO14)
 *    OLED SCL → D1 (GPIO5)
 *
 *  Touch → Emotion (tap to cycle forward):
 *    1 tap   HAPPY       ^_^  arc eyes, wide smile
 *    2 taps  ANGRY       >_<  inner triangle cuts, V scowl
 *    3 taps  SAD         ;_;  outer triangle cuts, frown
 *    4 taps  SURPRISED   O_O  tall eyes, open-O mouth
 *    5 taps  LOVE        hearts inside eyes, heart mouth
 *    6 taps  EXCITED     arc eyes bounce, zig-zag grin
 *    7 taps  SLEEPY      heavy lids, wavy line mouth
 *    8 taps  CONFUSED    squint left / wide right, wavy mouth
 *    9 taps  DISGUSTED   side-eye cut, curl mouth
 *   10 taps  NERVOUS     twitching small eyes, wavy mouth
 *   Hold    NEUTRAL     reset / wake from sleep
 *
 *  Auto:
 *    Look around randomly every 2–5 s
 *    Blink every 3–8 s
 *    No touch 30 s → YAWN → SLEEP (floating ZZZ)
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define TOUCH_PIN      14   // D5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── Emotions ─────────────────────────────────────────────────────
enum Emotion {
  NEUTRAL, HAPPY, ANGRY, SAD, SURPRISED,
  LOVE, EXCITED, SLEEPY, CONFUSED, DISGUSTED, NERVOUS,
  YAWNING, SLEEPING
};
Emotion currentEmotion = NEUTRAL;

// ── Movement ──────────────────────────────────────────────────────
int offsetX = 0, offsetY = 0;

// ── Timers ────────────────────────────────────────────────────────
unsigned long lastMove       = 0;
unsigned long lastBlink      = 0;
unsigned long lastTouchTime  = 0;
unsigned long emotionTimer   = 0;
unsigned long lastActivity   = 0;
unsigned long lastZzz        = 0;
unsigned long bounceClock    = 0;
unsigned long nervousClock   = 0;

// ── Touch ─────────────────────────────────────────────────────────
int  touchCount     = 0;
bool lastTouchState = LOW;
unsigned long holdStart = 0;
bool holdFired = false;

// ── Anim state ────────────────────────────────────────────────────
float zzzY       = 0;
int   bounceOff  = 0;
int   nervousOff = 0;
int   heartSize  = 6;
int   heartDir   = 1;
float yawnPhase  = 0;
bool  sleeping   = false;

#define SLEEP_AFTER_MS  30000UL

// ═══════════════════════════════════════════════════════════════
//  DRAW EYE  – same signature as original, style extended
// ═══════════════════════════════════════════════════════════════
void drawEye(int x, int y, int w, int h, bool isLeft) {

  switch (currentEmotion) {

    // HAPPY / EXCITED: top half cut → U-arc
    case HAPPY:
    case EXCITED:
      display.fillRoundRect(x, y, w, h, 20, SSD1306_WHITE);
      display.fillRect(x, y, w, h / 2 + 1, SSD1306_BLACK);
      break;

    // ANGRY: inner-top triangle cut (original)
    case ANGRY:
      display.fillRoundRect(x, y, w, h, 8, SSD1306_WHITE);
      if (isLeft) display.fillTriangle(x,   y, x+w, y,   x+w, y+h/2, SSD1306_BLACK);
      else        display.fillTriangle(x,   y, x+w, y,   x,   y+h/2, SSD1306_BLACK);
      break;

    // SAD: outer-top triangle cut → droopy inner corner
    case SAD:
      display.fillRoundRect(x, y, w, h, 8, SSD1306_WHITE);
      if (isLeft) display.fillTriangle(x,   y, x+w, y,   x,   y+h/2, SSD1306_BLACK);
      else        display.fillTriangle(x,   y, x+w, y,   x+w, y+h/2, SSD1306_BLACK);
      break;

    // SURPRISED: taller rounder oval
    case SURPRISED:
      display.fillRoundRect(x+2, y-5, w-4, h+10, 12, SSD1306_WHITE);
      break;

    // LOVE: normal eye + black heart punch-out
    case LOVE: {
      display.fillRoundRect(x, y, w, h, 10, SSD1306_WHITE);
      int hx = x + w/2, hy = y + h/2;
      int hs = heartSize;
      display.fillCircle(hx - hs/2, hy - hs/4, hs/2, SSD1306_BLACK);
      display.fillCircle(hx + hs/2, hy - hs/4, hs/2, SSD1306_BLACK);
      for (int i = 0; i <= hs; i++) {
        int hw2 = hs - i;
        if (hw2 < 1) break;
        display.drawFastHLine(hx - hw2, hy - hs/4 + i, hw2*2, SSD1306_BLACK);
      }
      break;
    }

    // SLEEPY: big top-lid cuts 60% leaving only bottom strip
    case SLEEPY:
    case YAWNING:
      display.fillRoundRect(x, y, w, h, 10, SSD1306_WHITE);
      display.fillRect(x, y, w, (h * 6) / 10, SSD1306_BLACK);
      break;

    // SLEEPING: thin slit only
    case SLEEPING:
      display.fillRoundRect(x, y + h/2 - 2, w, 5, 2, SSD1306_WHITE);
      break;

    // CONFUSED: left = squinted slit, right = wide
    case CONFUSED:
      if (isLeft)
        display.fillRoundRect(x, y + h/3, w, h - h/3, 6, SSD1306_WHITE);
      else
        display.fillRoundRect(x, y - 4, w, h + 5, 10, SSD1306_WHITE);
      break;

    // DISGUSTED: cut inner half → side-eye look
    case DISGUSTED:
      display.fillRoundRect(x, y, w, h, 8, SSD1306_WHITE);
      if (isLeft) display.fillRect(x + w/2, y, w - w/2 + 2, h, SSD1306_BLACK);
      else        display.fillRect(x,       y, w/2 + 2,     h, SSD1306_BLACK);
      break;

    // NERVOUS: small twitching eye
    case NERVOUS:
      display.fillRoundRect(x + 2 + nervousOff, y + 3, w - 4, h - 8, 7, SSD1306_WHITE);
      break;

    // NEUTRAL: original clean rect
    default:
      display.fillRoundRect(x, y, w, h, 10, SSD1306_WHITE);
      break;
  }
}

// ═══════════════════════════════════════════════════════════════
//  DRAW MOUTH
// ═══════════════════════════════════════════════════════════════
void drawMouth(int xOff, int yOff) {
  int cx = 64 + xOff / 2;
  int cy = 52 + yOff / 4;

  switch (currentEmotion) {

    // HAPPY: wide rounded smile (original style)
    case HAPPY:
      display.drawRoundRect(cx - 15, cy - 5, 30, 10, 5, SSD1306_WHITE);
      display.fillRect(cx - 16, cy - 6, 32, 7, SSD1306_BLACK);
      break;

    // ANGRY: V scowl (original style)
    case ANGRY:
      display.drawLine(cx-10, cy+2, cx,    cy-3, SSD1306_WHITE);
      display.drawLine(cx,    cy-3, cx+10, cy+2, SSD1306_WHITE);
      break;

    // SAD: inverted rounded rect (frown)
    case SAD:
      display.drawRoundRect(cx - 14, cy - 2, 28, 10, 5, SSD1306_WHITE);
      display.fillRect(cx - 15, cy + 3, 30, 7, SSD1306_BLACK);
      break;

    // SURPRISED: open O mouth
    case SURPRISED:
      display.drawRoundRect(cx - 6, cy - 5, 12, 12, 5, SSD1306_WHITE);
      break;

    // LOVE: white heart
    case LOVE: {
      int hs = 7;
      display.fillCircle(cx - hs/2, cy - hs/4, hs/2, SSD1306_WHITE);
      display.fillCircle(cx + hs/2, cy - hs/4, hs/2, SSD1306_WHITE);
      for (int i = 0; i <= hs; i++) {
        int hw2 = hs - i;
        if (hw2 < 1) break;
        display.drawFastHLine(cx - hw2, cy - hs/4 + i, hw2*2, SSD1306_WHITE);
      }
      break;
    }

    // EXCITED: zig-zag wide grin
    case EXCITED: {
      int startX = cx - 14;
      for (int i = 0; i < 7; i++) {
        int x1 = startX + i*4, y1 = cy + (i%2 == 0 ? 3 : 0);
        int x2 = startX + i*4 + 4, y2 = cy + (i%2 == 0 ? 0 : 3);
        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
      }
      break;
    }

    // SLEEPY: lazy small wavy line
    case SLEEPY:
      for (int i = -8; i <= 8; i++) {
        int wy = cy + (int)(sin(i * 0.6f) * 2);
        display.drawPixel(cx+i, wy,   SSD1306_WHITE);
        display.drawPixel(cx+i, wy+1, SSD1306_WHITE);
      }
      break;

    // YAWNING: big open mouth (drawn in render, skip here)
    case YAWNING:
      break;

    // SLEEPING: flat line + ZZZ
    case SLEEPING: {
      display.drawLine(cx-5, cy, cx+5, cy, SSD1306_WHITE);
      // Floating ZZZ
      int zy = (int)(cy - 4 - zzzY);
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      if (zy > 0) {
        display.setCursor(cx+10, zy);      display.print("z");
        display.setCursor(cx+16, zy - 6);  display.print("z");
        display.setCursor(cx+22, zy - 12); display.print("Z");
      }
      break;
    }

    // CONFUSED: wavy uneven line
    case CONFUSED:
      display.drawLine(cx-8, cy+2, cx-2, cy-2, SSD1306_WHITE);
      display.drawLine(cx-2, cy-2, cx+4, cy+2, SSD1306_WHITE);
      display.drawLine(cx+4, cy+2, cx+9, cy-1, SSD1306_WHITE);
      break;

    // DISGUSTED: lopsided curl
    case DISGUSTED:
      display.drawLine(cx-10, cy,    cx,    cy+3, SSD1306_WHITE);
      display.drawLine(cx,    cy+3,  cx+10, cy+1, SSD1306_WHITE);
      break;

    // NERVOUS: short wavy twitching line
    case NERVOUS: {
      for (int i = -6; i <= 6; i++) {
        int wy = cy + (int)(sin(i * 0.9f) * 2);
        display.drawPixel(cx + i + nervousOff, wy, SSD1306_WHITE);
      }
      break;
    }

    // NEUTRAL: small flat line (original)
    default:
      display.drawLine(cx-8, cy, cx+8, cy, SSD1306_WHITE);
      break;
  }
}

// ═══════════════════════════════════════════════════════════════
//  RENDER
// ═══════════════════════════════════════════════════════════════
void render() {
  display.clearDisplay();

  int eyeW = 28, eyeH = 30;
  int leftX  = 25 + offsetX;
  int rightX = 71 + offsetX;
  int yPos   = 10 + offsetY;

  // Excited bounces upward
  if (currentEmotion == EXCITED) yPos -= bounceOff;

  unsigned long now = millis();
  bool isBlink = (now - lastBlink < 130);

  // ── Eyes ──────────────────────────────────────────────────────
  if (isBlink && currentEmotion != SLEEPING && currentEmotion != SLEEPY
               && currentEmotion != YAWNING) {
    display.fillRect(leftX,  yPos + eyeH/2 - 2, eyeW, 4, SSD1306_WHITE);
    display.fillRect(rightX, yPos + eyeH/2 - 2, eyeW, 4, SSD1306_WHITE);
  } else {
    if (!isBlink && now - lastBlink > (unsigned long)random(3000, 8000))
      lastBlink = now;
    drawEye(leftX,  yPos, eyeW, eyeH, true);
    drawEye(rightX, yPos, eyeW, eyeH, false);
  }

  // ── Yawn mouth override ──────────────────────────────────────
  if (currentEmotion == YAWNING) {
    float yo = sin(yawnPhase);
    int mw = (int)(22 * yo) + 4;
    int mh = (int)(14 * yo) + 2;
    int cx = 64 + offsetX/2, cy = 52 + offsetY/4;
    if (mw > 4 && mh > 2) {
      display.fillRoundRect(cx-mw/2, cy-mh/2, mw, mh, mh/2, SSD1306_WHITE);
      if (mw > 10 && mh > 5)
        display.fillRoundRect(cx-mw/2+3, cy-mh/2+3, mw-6, mh-4, 2, SSD1306_BLACK);
    }
  } else {
    drawMouth(offsetX, offsetY);
  }

  display.display();
}

// ═══════════════════════════════════════════════════════════════
//  ANIMATIONS
// ═══════════════════════════════════════════════════════════════
void updateAnimations() {
  unsigned long now = millis();

  // Excited bounce
  if (currentEmotion == EXCITED) {
    if (now - bounceClock > 110) {
      bounceOff = (bounceOff == 0) ? 4 : 0;
      bounceClock = now;
    }
  } else bounceOff = 0;

  // Love heart pulse
  if (currentEmotion == LOVE) {
    if (now - bounceClock > 180) {
      heartSize += heartDir;
      if (heartSize > 8 || heartSize < 4) heartDir = -heartDir;
      bounceClock = now;
    }
  } else heartSize = 6;

  // Nervous twitch
  if (currentEmotion == NERVOUS) {
    if (now - nervousClock > 75) {
      nervousOff = random(-2, 3);
      nervousClock = now;
    }
  } else nervousOff = 0;

  // ZZZ float
  if (currentEmotion == SLEEPING) {
    if (now - lastZzz > 45) {
      zzzY += 0.35f;
      if (zzzY > 28) zzzY = 0;
      lastZzz = now;
    }
  } else zzzY = 0;
}

// ── Yawn → Sleep ──────────────────────────────────────────────
void runYawn() {
  currentEmotion = YAWNING;
  yawnPhase = 0;
  unsigned long start = millis();
  while (millis() - start < 2800) {
    float t = (millis() - start) / 2800.0f;
    yawnPhase = sin(t * PI);
    updateAnimations();
    render();
    delay(16);
  }
  yawnPhase = 0;
  currentEmotion = SLEEPING;
  sleeping = true;
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(4, 5);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  randomSeed(analogRead(A0));
  lastActivity = millis();
  lastBlink    = millis();
  Serial.println(F("Robo Face ready."));
}

// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();
  bool touchActive = (digitalRead(TOUCH_PIN) == HIGH);

  // ── Touch ─────────────────────────────────────────────────────
  if (touchActive && !lastTouchState) {
    holdStart = now;
    holdFired = false;
  }

  // Long hold → reset
  if (touchActive && lastTouchState && !holdFired) {
    if (now - holdStart > 700) {
      holdFired      = true;
      touchCount     = 0;
      sleeping       = false;
      currentEmotion = NEUTRAL;
      emotionTimer   = now;
      lastActivity   = now;
    }
  }

  // Tap (rising edge)
  if (touchActive && !lastTouchState && !holdFired) {
    touchCount++;
    lastTouchTime = now;
    lastActivity  = now;
    delay(40);
  }
  lastTouchState = touchActive;

  // Commit after tap window
  if (touchCount > 0 && !touchActive && (now - lastTouchTime > 380)) {
    if (sleeping) {
      sleeping       = false;
      currentEmotion = NEUTRAL;
    } else {
      int idx = ((touchCount - 1) % 10) + 1;
      switch (idx) {
        case 1:  currentEmotion = HAPPY;     break;
        case 2:  currentEmotion = ANGRY;     break;
        case 3:  currentEmotion = SAD;       break;
        case 4:  currentEmotion = SURPRISED; break;
        case 5:  currentEmotion = LOVE;      break;
        case 6:  currentEmotion = EXCITED;   break;
        case 7:  currentEmotion = SLEEPY;    break;
        case 8:  currentEmotion = CONFUSED;  break;
        case 9:  currentEmotion = DISGUSTED; break;
        case 10: currentEmotion = NERVOUS;   break;
      }
      emotionTimer = now;
      Serial.print(F("Emotion: ")); Serial.println(currentEmotion);
    }
    touchCount = 0;
  }

  // ── Return to neutral after 5 s ───────────────────────────────
  if (currentEmotion != NEUTRAL   && currentEmotion != SLEEPING &&
      currentEmotion != YAWNING   &&
      (now - emotionTimer > 5000)) {
    currentEmotion = NEUTRAL;
  }

  // ── Look around ───────────────────────────────────────────────
  if (!sleeping && (now - lastMove > (unsigned long)random(2000, 5000))) {
    offsetX  = random(-1, 2) * 10;
    offsetY  = random(-1, 2) * 5;
    lastMove = now;
  }

  updateAnimations();
  render();
}