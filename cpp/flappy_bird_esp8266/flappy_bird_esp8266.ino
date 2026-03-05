#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Touch Sensor Pin (GPIO4 / D2 on ESP8266)
#define TOUCH_PIN 14

// Game Constants
#define BIRD_SIZE 4
#define PIPE_WIDTH 20
#define PIPE_GAP 25
#define GRAVITY 0.4f
#define FLAP_STRENGTH -3.0f

// Game States
enum GameState {
  MENU,
  PLAYING,
  GAME_OVER
};

// Bird structure
struct Bird {
  float x;
  float y;
  float velocity;
} bird;

// Pipe structure
struct Pipe {
  int x;
  int gap_y;
  bool passed;
} pipes[3];

// Game variables
GameState gameState = MENU;
int score = 0;
int highScore = 0;
unsigned long lastFlap = 0;
unsigned long lastPipeGen = 0;
bool touchPressed = false;
bool lastTouchState = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\nFlappy Bird ESP8266 Starting...");
  
  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Flappy Bird");
  display.println("ESP8266");
  display.display();
  delay(2000);
  
  // Initialize touch pin
  pinMode(TOUCH_PIN, INPUT);
  
  // Initialize game
  initializeGame();
  
  Serial.println("Setup complete!");
}

void loop() {
  // Read touch sensor
  touchPressed = digitalRead(TOUCH_PIN);
  
  // Handle state machine
  switch (gameState) {
    case MENU:
      drawMenu();
      if (touchPressed && !lastTouchState) {
        gameState = PLAYING;
        initializeGame();
      }
      break;
      
    case PLAYING:
      updateGame();
      drawGame();
      
      // Check if touch detected (rising edge)
      if (touchPressed && !lastTouchState) {
        flapBird();
      }
      break;
      
    case GAME_OVER:
      drawGameOver();
      if (touchPressed && !lastTouchState) {
        gameState = MENU;
      }
      break;
  }
  
  lastTouchState = touchPressed;
  delay(20); // ~50 FPS
}

void initializeGame() {
  // Initialize bird
  bird.x = 20;
  bird.y = SCREEN_HEIGHT / 2;
  bird.velocity = 0;
  
  // Initialize pipes
  for (int i = 0; i < 3; i++) {
    pipes[i].x = SCREEN_WIDTH + i * 60;
    pipes[i].gap_y = random(15, SCREEN_HEIGHT - 15 - PIPE_GAP);
    pipes[i].passed = false;
  }
  
  score = 0;
  lastFlap = millis();
  lastPipeGen = millis();
}

void flapBird() {
  bird.velocity = FLAP_STRENGTH;
  lastFlap = millis();
  Serial.println("Flap!");
}

void updateGame() {
  // Apply gravity
  bird.velocity += GRAVITY;
  bird.y += bird.velocity;
  
  // Clamp bird position (don't go off screen vertically)
  if (bird.y < BIRD_SIZE) {
    bird.y = BIRD_SIZE;
    bird.velocity = 0;
    endGame();
    return;
  }
  if (bird.y > SCREEN_HEIGHT - BIRD_SIZE) {
    bird.y = SCREEN_HEIGHT - BIRD_SIZE;
    endGame();
    return;
  }
  
  // Update pipes
  for (int i = 0; i < 3; i++) {
    pipes[i].x -= 3; // Move pipe left
    
    // Check if pipe is off screen
    if (pipes[i].x < -PIPE_WIDTH) {
      pipes[i].x = SCREEN_WIDTH;
      pipes[i].gap_y = random(15, SCREEN_HEIGHT - 15 - PIPE_GAP);
      pipes[i].passed = false;
    }
    
    // Check collision with pipe
    if (checkCollision(i)) {
      endGame();
      return;
    }
    
    // Award point when bird passes pipe
    if (!pipes[i].passed && pipes[i].x < bird.x && pipes[i].x + PIPE_WIDTH > bird.x - 5) {
      pipes[i].passed = true;
      score++;
      Serial.print("Score: ");
      Serial.println(score);
    }
  }
}

bool checkCollision(int pipeIndex) {
  // Bird bounds
  int bird_left = bird.x - BIRD_SIZE / 2;
  int bird_right = bird.x + BIRD_SIZE / 2;
  int bird_top = bird.y - BIRD_SIZE / 2;
  int bird_bottom = bird.y + BIRD_SIZE / 2;
  
  // Pipe bounds
  int pipe_left = pipes[pipeIndex].x;
  int pipe_right = pipes[pipeIndex].x + PIPE_WIDTH;
  int gap_top = pipes[pipeIndex].gap_y;
  int gap_bottom = pipes[pipeIndex].gap_y + PIPE_GAP;
  
  // Check if bird overlaps with pipe (not in the gap)
  if (bird_right > pipe_left && bird_left < pipe_right) {
    // Check if bird is not in the gap
    if (bird_top < gap_top || bird_bottom > gap_bottom) {
      return true;
    }
  }
  
  return false;
}

void endGame() {
  if (score > highScore) {
    highScore = score;
  }
  gameState = GAME_OVER;
  Serial.print("Game Over! Score: ");
  Serial.print(score);
  Serial.print(" | High Score: ");
  Serial.println(highScore);
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 5);
  display.println("FLAPPY");
  display.setCursor(20, 25);
  display.println("BIRD");
  
  display.setTextSize(1);
  display.setCursor(25, 45);
  display.println("Touch to Play!");
  
  display.setTextSize(1);
  display.setCursor(15, 58);
  display.print("High Score: ");
  display.println(highScore);
  
  display.display();
}

void drawGame() {
  display.clearDisplay();
  
  // Draw score
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Score: ");
  display.println(score);
  
  // Draw bird
  display.fillCircle(bird.x, bird.y, BIRD_SIZE / 2, SSD1306_WHITE);
  
  // Draw pipes
  for (int i = 0; i < 3; i++) {
    // Top pipe
    display.fillRect(pipes[i].x, 0, PIPE_WIDTH, pipes[i].gap_y, SSD1306_WHITE);
    
    // Bottom pipe
    display.fillRect(pipes[i].x, pipes[i].gap_y + PIPE_GAP, PIPE_WIDTH, 
                     SCREEN_HEIGHT - (pipes[i].gap_y + PIPE_GAP), SSD1306_WHITE);
  }
  
  display.display();
}

void drawGameOver() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 10);
  display.println("GAME");
  display.setCursor(25, 28);
  display.println("OVER");
  
  display.setTextSize(1);
  display.setCursor(20, 45);
  display.print("Score: ");
  display.println(score);
  
  display.setCursor(10, 55);
  display.print("High: ");
  display.println(highScore);
  
  display.display();
}
