/*
 * ╔══════════════════════════════════════════════════════════════════╗
 *  DINO RUN  –  ESP8266 + 0.96" I2C OLED 128×64 + Touch Sensor
 * ╠══════════════════════════════════════════════════════════════════╣
 *  Wiring:
 *    OLED VCC  → 3.3V          Touch VCC → 3.3V
 *    OLED GND  → GND           Touch GND → GND
 *    OLED SDA  → D2 (GPIO 4)   Touch SIG → D5 (GPIO 14)
 *    OLED SCL  → D1 (GPIO 5)
 *
 *  Controls:
 *    Tap touch sensor  → JUMP  (also starts game / restarts after death)
 *
 *  Libraries needed  (Arduino Library Manager):
 *    • Adafruit SSD1306
 *    • Adafruit GFX Library
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Hardware ────────────────────────────────────────────────────────
#define SCREEN_W   128
#define SCREEN_H    64
#define OLED_ADDR  0x3C
#define TOUCH_PIN   14        // D5

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ── Ground / scene constants ────────────────────────────────────────
#define GND_Y        52       // top of ground line (dino feet rest here)
#define GND_LINE_Y   53       // drawn ground line

// ── Dino geometry ───────────────────────────────────────────────────
#define DINO_X        12      // fixed horizontal position
#define DINO_W        14      // collision box width
#define DINO_H_NORM   18      // standing height
#define DINO_H_DUCK   10      // ducking height  (not used as obstacle, kept for future)

// ── Physics ─────────────────────────────────────────────────────────
#define GRAVITY       2       // pixels/frame² (integer, applied every PHYS_STEP)
#define JUMP_VEL     -14      // initial upward velocity (negative = up)
#define PHYS_EVERY    1       // apply physics every N frames

// ── Cactus pool ─────────────────────────────────────────────────────
#define MAX_CACTI     3
struct Cactus {
  int  x;
  int  h;          // cactus height
  int  w;          // cactus width
  bool active;
};
Cactus cacti[MAX_CACTI];

// ── Cloud pool ───────────────────────────────────────────────────────
#define MAX_CLOUDS    3
struct Cloud {
  int  x, y;
  bool active;
};
Cloud clouds[MAX_CLOUDS];

// ── Scrolling ground dots ────────────────────────────────────────────
#define NUM_DOTS  10
int dotX[NUM_DOTS];

// ── Game state ───────────────────────────────────────────────────────
enum State { WAITING, PLAYING, DEAD };
State state = WAITING;

int   dinoY      = GND_Y;     // Y of dino BOTTOM (feet)
int   dinoVY     = 0;         // vertical velocity
bool  jumping    = false;

unsigned int  score    = 0;
unsigned int  hiScore  = 0;
unsigned long lastFrame = 0;
int           frameCount = 0;

// Speed: starts at 2 px/frame, increases every 200 points
int gameSpeed() {
  int s = 2 + (int)(score / 200);
  return min(s, 6);
}

// ── Spawn intervals ──────────────────────────────────────────────────
int  nextCactus = 0;    // frames until next cactus
int  nextCloud  = 0;

// ── Touch debounce ───────────────────────────────────────────────────
bool          lastTouch    = false;
unsigned long lastTouchMs  = 0;
#define DEBOUNCE_MS  80

// ── Leg animation ────────────────────────────────────────────────────
int  legPhase = 0;         // 0 or 1
int  legTick  = 0;

// ═══════════════════════════════════════════════════════════════════
//  PIXEL-ART DINO  (hand-crafted 14×18 bitmap, LSB = leftmost pixel)
//
//  Each row is a uint16_t where bit15 = leftmost pixel (x=0)
//  Two bitmaps: leg frame 0 and leg frame 1
// ═══════════════════════════════════════════════════════════════════
// Dino sprite rows (14 wide, 18 tall)
// Bit order: bit 13 = col 0 (left), bit 0 = col 13 (right)
// We'll draw it manually with fillRect calls — more readable on small display

void drawDino(int bx, int by, int legF, bool isDead) {
  // bx, by = top-left of 14×18 bounding box
  // All coordinates relative to bx, by

  uint16_t col = SSD1306_WHITE;

  // ── Head (5×5 at top-right) ──────────────────────────────────
  display.fillRect(bx+6, by,   8, 6, col);   // head block
  display.fillRect(bx+11,by+1, 3, 2, col);   // snout
  // Eye (1×1 black cutout inside head)
  display.drawPixel(bx+8, by+2, SSD1306_BLACK);

  // ── Neck / body ──────────────────────────────────────────────
  display.fillRect(bx+4, by+5, 6, 4, col);   // neck
  display.fillRect(bx+2, by+8, 10,6, col);   // main body
  display.fillRect(bx+0, by+10,4, 4, col);   // tail base
  display.fillRect(bx-2, by+13,3, 2, col);   // tail tip (may go -x, clip OK)

  // ── Tiny arm ─────────────────────────────────────────────────
  display.fillRect(bx+8, by+8, 3, 2, col);

  // ── Legs (animated) ──────────────────────────────────────────
  if (jumping) {
    // Tucked legs
    display.fillRect(bx+3, by+13, 3, 4, col);
    display.fillRect(bx+7, by+13, 3, 4, col);
  } else if (legF == 0) {
    // Stride A
    display.fillRect(bx+3, by+13, 3, 6, col); // front leg down
    display.fillRect(bx+3, by+18, 4, 2, col); // front foot
    display.fillRect(bx+7, by+13, 3, 4, col); // back leg mid
  } else {
    // Stride B
    display.fillRect(bx+3, by+13, 3, 4, col); // front leg mid
    display.fillRect(bx+7, by+13, 3, 6, col); // back leg down
    display.fillRect(bx+7, by+18, 4, 2, col); // back foot
  }

  // ── Death X eyes ─────────────────────────────────────────────
  if (isDead) {
    int ex = bx+7, ey = by+2;
    display.drawLine(ex,   ey,   ex+2, ey+2, SSD1306_BLACK);
    display.drawLine(ex+2, ey,   ex,   ey+2, SSD1306_BLACK);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  CACTUS  – two-arm cactus drawn with rects
// ═══════════════════════════════════════════════════════════════════
void drawCactus(int x, int h, int w) {
  int base = GND_LINE_Y;       // cactus feet sit on ground line
  int top  = base - h;

  int cx = x + w/2 - 2;       // trunk x

  // Trunk
  display.fillRect(cx, top, 4, h, SSD1306_WHITE);

  // Left arm
  int armY = top + h/3;
  display.fillRect(x,   armY,    cx - x, 3, SSD1306_WHITE);  // horizontal
  display.fillRect(x,   armY-6,  3, 9,      SSD1306_WHITE);  // vertical tip

  // Right arm
  int rArmX = cx + 4;
  int rEndX = x + w;
  display.fillRect(rArmX, armY+4, rEndX - rArmX, 3, SSD1306_WHITE);
  display.fillRect(rEndX-3, armY-2, 3, 9, SSD1306_WHITE);
}

// ═══════════════════════════════════════════════════════════════════
//  CLOUD
// ═══════════════════════════════════════════════════════════════════
void drawCloud(int x, int y) {
  display.drawRoundRect(x,    y+4,  20, 7, 3, SSD1306_WHITE);
  display.drawRoundRect(x+4,  y,    14, 8, 3, SSD1306_WHITE);
}

// ═══════════════════════════════════════════════════════════════════
//  SCORE  (top-right, tiny text)
// ═══════════════════════════════════════════════════════════════════
void drawScore() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // HI
  char hibuf[12];
  snprintf(hibuf, sizeof(hibuf), "HI%05u", hiScore);
  display.setCursor(62, 0);
  display.print(hibuf);

  // Current
  char buf[8];
  snprintf(buf, sizeof(buf), "%05u", score);
  display.setCursor(94, 0);
  display.print(buf);
}

// ═══════════════════════════════════════════════════════════════════
//  SCREENS
// ═══════════════════════════════════════════════════════════════════
void drawStartScreen() {
  display.clearDisplay();
  // Ground line
  display.drawFastHLine(0, GND_LINE_Y, SCREEN_W, SSD1306_WHITE);
  // Static dino
  drawDino(DINO_X, GND_Y - DINO_H_NORM, 0, false);
  // Message
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(36, 20);
  display.print(F("DINO  RUN"));
  display.setCursor(28, 34);
  display.print(F("TAP TO START"));
  display.display();
}

void drawDeadScreen() {
  // Keep the last game frame visible, just add overlay text
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(34, 20);
  display.print(F("GAME OVER"));
  display.setCursor(16, 32);
  display.print(F("TAP TO RESTART"));
  display.display();
}

// ═══════════════════════════════════════════════════════════════════
//  SPAWN HELPERS
// ═══════════════════════════════════════════════════════════════════
void spawnCactus() {
  for (int i = 0; i < MAX_CACTI; i++) {
    if (!cacti[i].active) {
      cacti[i].active = true;
      cacti[i].x      = SCREEN_W + 4;
      cacti[i].h      = 14 + random(0, 12);   // 14–26 px tall
      cacti[i].w      = 12 + random(0, 6);    // 12–18 px wide
      return;
    }
  }
}

void spawnCloud() {
  for (int i = 0; i < MAX_CLOUDS; i++) {
    if (!clouds[i].active) {
      clouds[i].active = true;
      clouds[i].x      = SCREEN_W + 4;
      clouds[i].y      = 4 + random(0, 18);
      return;
    }
  }
}

void initDots() {
  for (int i = 0; i < NUM_DOTS; i++)
    dotX[i] = random(0, SCREEN_W);
}

// ═══════════════════════════════════════════════════════════════════
//  COLLISION
// ═══════════════════════════════════════════════════════════════════
bool checkCollision() {
  // Dino bounding box (generous 2px shrink for fairness)
  int dL  = DINO_X + 4;
  int dR  = DINO_X + DINO_W - 2;
  int dT  = dinoY  - DINO_H_NORM + 4;
  int dB  = dinoY;

  for (int i = 0; i < MAX_CACTI; i++) {
    if (!cacti[i].active) continue;
    int cL = cacti[i].x + 2;
    int cR = cacti[i].x + cacti[i].w - 2;
    int cT = GND_LINE_Y - cacti[i].h + 2;
    int cB = GND_LINE_Y;

    if (dR > cL && dL < cR && dB > cT && dT < cB)
      return true;
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════
//  GAME RESET
// ═══════════════════════════════════════════════════════════════════
void resetGame() {
  dinoY    = GND_Y;
  dinoVY   = 0;
  jumping  = false;
  score    = 0;
  frameCount = 0;
  legPhase = 0;
  legTick  = 0;
  nextCactus = 50;
  nextCloud  = 30;

  for (int i = 0; i < MAX_CACTI; i++) cacti[i].active = false;
  for (int i = 0; i < MAX_CLOUDS; i++) clouds[i].active = false;
  initDots();
  state = PLAYING;
}

// ═══════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(4, 5);   // SDA=D2, SCL=D1

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED not found!"));
    while (true) delay(100);
  }

  display.setRotation(0);
  display.clearDisplay();
  randomSeed(analogRead(A0));

  for (int i = 0; i < MAX_CACTI;  i++) cacti[i].active  = false;
  for (int i = 0; i < MAX_CLOUDS; i++) clouds[i].active = false;
  initDots();

  drawStartScreen();
  Serial.println(F("Dino Run ready. Tap D5 to start."));
}

// ═══════════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════════
void loop() {
  // ── Frame rate cap: ~40 fps (25 ms/frame) ──────────────────────
  unsigned long now = millis();
  if (now - lastFrame < 25) return;
  lastFrame = now;

  // ── Touch read & debounce ───────────────────────────────────────
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);
  bool justTapped = false;
  if (touched && !lastTouch && (now - lastTouchMs > DEBOUNCE_MS)) {
    justTapped   = true;
    lastTouchMs  = now;
  }
  lastTouch = touched;

  // ── State machine ───────────────────────────────────────────────
  switch (state) {

    // ── WAITING: show start screen, tap to begin ──────────────────
    case WAITING:
      if (justTapped) {
        resetGame();
      }
      break;

    // ── DEAD: show game-over, tap to restart ──────────────────────
    case DEAD:
      if (justTapped) {
        resetGame();
      }
      break;

    // ── PLAYING ───────────────────────────────────────────────────
    case PLAYING: {
      int spd = gameSpeed();
      frameCount++;

      // Jump on tap
      if (justTapped && !jumping) {
        dinoVY  = JUMP_VEL;
        jumping = true;
      }

      // ── Physics ──────────────────────────────────────────────────
      if (jumping) {
        dinoVY += GRAVITY;
        dinoY  += dinoVY;
        if (dinoY >= GND_Y) {
          dinoY   = GND_Y;
          dinoVY  = 0;
          jumping = false;
        }
      }

      // ── Leg animation (only when on ground) ───────────────────────
      if (!jumping) {
        legTick++;
        if (legTick >= (6 - spd + 1)) {   // faster legs at higher speed
          legTick  = 0;
          legPhase = 1 - legPhase;
        }
      }

      // ── Spawn cactus ──────────────────────────────────────────────
      nextCactus--;
      if (nextCactus <= 0) {
        spawnCactus();
        // Gap between cacti shortens as speed increases
        nextCactus = (int)(60 / spd) + random(10, 30);
      }

      // ── Spawn cloud ───────────────────────────────────────────────
      nextCloud--;
      if (nextCloud <= 0) {
        spawnCloud();
        nextCloud = 40 + random(0, 30);
      }

      // ── Move cacti ────────────────────────────────────────────────
      for (int i = 0; i < MAX_CACTI; i++) {
        if (!cacti[i].active) continue;
        cacti[i].x -= spd;
        if (cacti[i].x + cacti[i].w < 0) cacti[i].active = false;
      }

      // ── Move clouds ───────────────────────────────────────────────
      for (int i = 0; i < MAX_CLOUDS; i++) {
        if (!clouds[i].active) continue;
        clouds[i].x -= 1;    // clouds drift slowly
        if (clouds[i].x + 26 < 0) clouds[i].active = false;
      }

      // ── Scroll ground dots ────────────────────────────────────────
      for (int i = 0; i < NUM_DOTS; i++) {
        dotX[i] -= spd;
        if (dotX[i] < 0) dotX[i] = SCREEN_W + random(0, 10);
      }

      // ── Score ─────────────────────────────────────────────────────
      if (frameCount % 5 == 0) {   // increment score every 5 frames
        score++;
        if (score > hiScore) hiScore = score;
      }

      // ── Collision check ───────────────────────────────────────────
      if (checkCollision()) {
        state = DEAD;
        // Draw final frame with game-over text then return
        display.clearDisplay();
        // Ground
        display.drawFastHLine(0, GND_LINE_Y, SCREEN_W, SSD1306_WHITE);
        // Ground dots
        for (int i = 0; i < NUM_DOTS; i++)
          display.drawPixel(dotX[i], GND_LINE_Y + 2 + (i % 3), SSD1306_WHITE);
        // Clouds
        for (int i = 0; i < MAX_CLOUDS; i++)
          if (clouds[i].active) drawCloud(clouds[i].x, clouds[i].y);
        // Cacti
        for (int i = 0; i < MAX_CACTI; i++)
          if (cacti[i].active) drawCactus(cacti[i].x, cacti[i].h, cacti[i].w);
        // Dead dino
        drawDino(DINO_X, dinoY - DINO_H_NORM, legPhase, true);
        // Score
        drawScore();
        // Game over text
        drawDeadScreen();
        return;
      }

      // ── Draw frame ────────────────────────────────────────────────
      display.clearDisplay();

      // Stars (sparse dots in sky for flair)
      if (score > 300) {
        // Show stars after score 300 for atmosphere
        for (int i = 0; i < 6; i++) {
          int sx = (i * 23 + frameCount / 3) % SCREEN_W;
          int sy = 2 + (i * 7) % 14;
          display.drawPixel(sx, sy, SSD1306_WHITE);
        }
      }

      // Ground line
      display.drawFastHLine(0, GND_LINE_Y, SCREEN_W, SSD1306_WHITE);

      // Ground pebble dots
      for (int i = 0; i < NUM_DOTS; i++)
        display.drawPixel(dotX[i], GND_LINE_Y + 2 + (i % 3), SSD1306_WHITE);

      // Clouds
      for (int i = 0; i < MAX_CLOUDS; i++)
        if (clouds[i].active) drawCloud(clouds[i].x, clouds[i].y);

      // Cacti
      for (int i = 0; i < MAX_CACTI; i++)
        if (cacti[i].active) drawCactus(cacti[i].x, cacti[i].h, cacti[i].w);

      // Dino
      drawDino(DINO_X, dinoY - DINO_H_NORM, legPhase, false);

      // Score
      drawScore();

      display.display();
      break;
    }
  }
}
