#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TOUCH_PIN 14 // D5
// Pico Pin Definitions
#define SDA_PIN 4  
#define SCL_PIN 5  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

enum Emotion { NEUTRAL, HAPPY, ANGRY, CONFUSED, SAD, SLEEPING};
Emotion currentEmotion = NEUTRAL;

int offsetX = 0, offsetY = 0;
unsigned long lastMove = 0, lastBlink = 0, lastTouchTime = 0, emotionTimer = 0;
int touchCount = 0;
bool lastTouchState = LOW;

void setup() {
 Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);

  // 1. Initialize I2C pins
  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
  Wire.begin();

  // 2. INITIALIZE THE DISPLAY (This is likely what's missing)
  // 0x3C is the most common I2C address for these 128x64 OLEDs
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  display.display();
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
  } else if (currentEmotion == SAD) {
    // A downward curve (frown)
    display.drawRoundRect(centerX - 12, centerY + 2, 24, 10, 5, SSD1306_WHITE);
    display.fillRect(centerX - 13, centerY + 7, 26, 6, SSD1306_BLACK); // Cut bottom to leave a frown
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
  } else if (currentEmotion == SAD) {
  display.fillRoundRect(x, y + 8, w, h - 12, 10, SSD1306_WHITE); 
    if (x < 64) display.fillRect(x + 5, y + h - 2, 3, 3, SSD1306_WHITE); 
  }
  else {
    display.fillRoundRect(x, y, w, h, 10, SSD1306_WHITE);
  }
}
void render() {
  display.clearDisplay();
  unsigned long now = millis();
  
  // Calculate a "bobbing" offset (moves between -3 and 3 pixels)
  // 1000.0 controls the speed; lower is faster.
  int bobbing = sin(now / 200.0) * 3; 

  int eyeW = 28;
  int eyeH = 32;
  
  int leftX = 25 + offsetX;
  int rightX = 71 + offsetX;
  int yPos = 10 + offsetY; // Eyes positioned higher to leave room for mouth
  
  if (currentEmotion == SLEEPING) {
    display.fillRect(leftX, 25 + offsetY, eyeW, 4, SSD1306_WHITE);
    display.fillRect(rightX, 25 + offsetY, eyeW, 4, SSD1306_WHITE);
    int zCount = (now / 500) % 3; 
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    if (zCount >= 0) { display.setCursor(105, 20 + bobbing); display.print("z"); }
    if (zCount >= 1) { display.setCursor(112, 12 + bobbing); display.print("z"); }
    if (zCount >= 2) { display.setCursor(120, 4 + bobbing); display.print("Z"); }
    display.drawFastHLine(60 + (offsetX/2), 55, 8, SSD1306_WHITE);
  } else {
  // Draw Eyes (with Blink logic)
  if (now - lastBlink < 150) {
    display.fillRect(leftX, 25 + offsetY, eyeW, 4, SSD1306_WHITE);
    display.fillRect(rightX, 25 + offsetY, eyeW, 4, SSD1306_WHITE);
  } else {
    drawEye(leftX, yPos, eyeW, eyeH);
    drawEye(rightX, yPos, eyeW, eyeH);
    if (now - lastBlink > random(3000, 8000)) lastBlink = now;
  }

  if (currentEmotion == CONFUSED) {
    // Draw a "?" above the head
    display.setCursor(60, 2 + bobbing);
    display.setTextSize(2); // Make it big
    display.setTextColor(SSD1306_WHITE);
    display.print("?");
    
    // Confused mouth: A small wavy line or zig-zag
    display.drawLine(58, 52, 62, 55, SSD1306_WHITE);
    display.drawLine(62, 55, 66, 52, SSD1306_WHITE);
    display.drawLine(66, 52, 70, 55, SSD1306_WHITE);
  } else {
      drawMouth(offsetX, offsetY);
  }
  }
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
    if (touchCount == 1) {
      currentEmotion = HAPPY;
    } else if (touchCount == 2) {
      currentEmotion = ANGRY;
    } else if (touchCount == 3) {
      currentEmotion = SAD;
      offsetX = 0; offsetY = 10;   // Look down at the floor
    }
     else if (touchCount == 4) {
      currentEmotion = SLEEPING;
    } else {
      currentEmotion = CONFUSED;
      offsetX = 0; offsetY = 5;    // Look down slightly
    }
    
    touchCount = 0;
    emotionTimer = now;
  }

  if (currentEmotion != NEUTRAL && currentEmotion != SLEEPING && (now - emotionTimer > 4000)) {
    currentEmotion = NEUTRAL;
  }

  // Look Around Logic
  if (currentEmotion != SLEEPING && (now - lastMove > random(2000, 5000))) {
      offsetX = random(-1, 2) * 10;
      offsetY = random(-1, 2) * 5;
      lastMove = now;
    }

  render();
}