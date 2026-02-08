#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Access Point Config ---
const char* ssid = "esp_bot"; // No password

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TOUCH_PIN 14 // D5 on NodeMCU
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
ESP8266WebServer server(80);

// --- State Variables ---
String currentMsg = "";
unsigned long msgTimer = 0;
unsigned long lastTouchTime = 0;
int lookX = 0, lookY = 0;
bool isSleeping = false;

// --- Web Interface ---
void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{text-align:center; font-family:sans-serif; background:#f0f0f0; padding-top:50px;}";
  html += "input{padding:10px; font-size:18px; border-radius:5px; border:1px solid #ccc;}";
  html += "input[type='submit']{background:#007bff; color:white; border:none; cursor:pointer;}</style></head>";
  html += "<body><h1>Message your Bot</h1>";
  html += "<form action='/msg'><input type='text' name='val' placeholder='Type here...'><br><br>";
  html += "<input type='submit' value='Send Message'></form></body></html>";
  server.send(200, "text/html", html);
}

void handleMsg() {
  currentMsg = server.arg("val");
  msgTimer = millis(); 
  server.send(200, "text/html", "<h2>Sent! Check the screen.</h2><script>setTimeout(function(){window.location.href='/';}, 2000);</script>");
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(4, 5); // D2=SDA, D1=SCL

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  display.clearDisplay();

  // --- Start Access Point ---
  WiFi.softAP(ssid); 
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP); // Default is 192.168.4.1

  server.on("/", handleRoot);
  server.on("/msg", handleMsg);
  server.begin();
}

void drawEyes(int ox, int oy, bool blink, bool thinking = false) {
  int ey = 22 + oy;
  if (blink) {
    display.drawFastHLine(25 + ox, ey + 7, 20, WHITE);
    display.drawFastHLine(83 + ox, ey + 7, 20, WHITE);
  } else {
    display.fillRoundRect(25 + ox, ey, 20, 14, 6, WHITE);
    display.fillRoundRect(83 + ox, ey, 20, 14, 6, WHITE);
    if (thinking) { 
       display.setCursor(110, 5); display.print("?");
    }
  }
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  // 1. Interactive Logic (Touch)
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);
  if (touched) {
    lastTouchTime = now;
    isSleeping = false;
  }

  display.clearDisplay();

  // 2. Priority 1: Web Message (for 6 seconds)
  if (currentMsg != "" && (now - msgTimer < 6000)) {
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(5, 20);
    display.println(currentMsg.substring(0, 10)); // Limits to 10 chars to fit
  } 
  // 3. Priority 2: Sleep Mode (after 15 seconds)
  else if (now - lastTouchTime > 60000) {
    isSleeping = true;
    display.setCursor(50, 25);
    display.setTextSize(2);
    display.print("Zzz");
    if (random(0, 1000) < 5) lastTouchTime = now; // Random wake up
  }
  // 4. Priority 3: Idle Face (Looking around)
  else {
    bool blink = (random(0, 100) < 5);
    
    // Change gaze direction randomly
    if (random(0, 100) < 5) { 
      lookX = random(-4, 5); 
      lookY = random(-2, 3); 
    }

    if (touched) { // Happy Face
      drawEyes(lookX, lookY - 3, blink);
      display.drawLine(44, 48, 54, 56, WHITE);
      display.drawLine(54, 56, 74, 56, WHITE);
      display.drawLine(74, 56, 84, 48, WHITE);
    } else { // Neutral Looking around
      drawEyes(lookX, lookY, blink, (random(0,200) < 2));
      display.drawFastHLine(44, 52, 40, WHITE);
    }
  }

  display.display();
  delay(50);
}