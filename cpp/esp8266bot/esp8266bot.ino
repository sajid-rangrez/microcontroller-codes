#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TOUCH_PIN 14 // D5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

enum Emotion { NEUTRAL, HAPPY, ANGRY };
Emotion currentEmotion = NEUTRAL;

int offsetX = 0, offsetY = 0;
unsigned long lastMove = 0, lastBlink = 0, lastTouchTime = 0, emotionTimer = 0;
int touchCount = 0;
bool lastTouchState = LOW;

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(4, 5); // SDA=D2, SCL=D1
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
}

void drawMouth(int xOff, int yOff) {
  int centerX = 64 + (xOff / 2); // Mouth moves less than eyes for depth
  int centerY = 52 + (yOff / 4);
  
  if (currentEmotion == HAPPY) {
    // A nice wide smile
    display.drawRoundRect(centerX - 15, centerY - 5, 30, 10, 5, SSD1306_WHITE);
    display.fillRect(centerX - 16, centerY - 6, 32, 6, SSD1306_BLACK); // Cut top
  } 
  else if (currentEmotion == ANGRY) {
    // An inverted V/scowl
    display.drawLine(centerX - 10, centerY + 2, centerX, centerY - 3, SSD1306_WHITE);
    display.drawLine(centerX, centerY - 3, centerX + 10, centerY + 2, SSD1306_WHITE);
  } 
  else {
    // Neutral: Just a small flat line
    display.drawLine(centerX - 8, centerY, centerX + 8, centerY, SSD1306_WHITE);
  }
}

void drawEye(int x, int y, int w, int h) {
  if (currentEmotion == HAPPY) {
    display.fillRoundRect(x, y, w, h, 20, SSD1306_WHITE);
    display.fillRect(x, y + (h/2), w, h/2, SSD1306_BLACK); 
  } 
  else if (currentEmotion == ANGRY) {
    display.fillRoundRect(x, y, w, h, 8, SSD1306_WHITE);
    if (x < 64) display.fillTriangle(x, y, x+w, y, x+w, y+(h/2), SSD1306_BLACK);
    else display.fillTriangle(x, y, x+w, y, x, y+(h/2), SSD1306_BLACK);
  } 
  else {
    display.fillRoundRect(x, y, w, h, 10, SSD1306_WHITE);
  }
}

void render() {
  display.clearDisplay();
  int eyeW = 28;
  int eyeH = 32;
  
  int leftX = 25 + offsetX;
  int rightX = 71 + offsetX;
  int yPos = 10 + offsetY; // Eyes positioned higher to leave room for mouth

  unsigned long now = millis();
  
  // Draw Eyes (with Blink logic)
  if (now - lastBlink < 150) {
    display.fillRect(leftX, 25 + offsetY, eyeW, 4, SSD1306_WHITE);
    display.fillRect(rightX, 25 + offsetY, eyeW, 4, SSD1306_WHITE);
  } else {
    drawEye(leftX, yPos, eyeW, eyeH);
    drawEye(rightX, yPos, eyeW, eyeH);
    if (now - lastBlink > random(3000, 8000)) lastBlink = now;
  }

  // Draw Mouth
  drawMouth(offsetX, offsetY);
  
  display.display();
}

void loop() {
  unsigned long now = millis();
  bool touchActive = digitalRead(TOUCH_PIN);

  // Touch Logic
  if (touchActive == HIGH && lastTouchState == LOW) {
    touchCount++;
    lastTouchTime = now;
    delay(50); 
  }
  lastTouchState = touchActive;

  if (touchCount > 0 && (now - lastTouchTime > 350)) {
    if (touchCount == 1) currentEmotion = HAPPY;
    else currentEmotion = ANGRY;
    touchCount = 0;
    emotionTimer = now;
  }

  if (currentEmotion != NEUTRAL && (now - emotionTimer > 4000)) {
    currentEmotion = NEUTRAL;
  }

  // Look Around Logic
  if (now - lastMove > random(2000, 5000)) {
    offsetX = random(-1, 2) * 10;
    offsetY = random(-1, 2) * 5;
    lastMove = now;
  }

  render();
}