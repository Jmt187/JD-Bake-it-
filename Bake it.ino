/*
 * BAKE IT! - Junior Design Project
 * A Bop-it style game with baking theme for ATmega328P
 * 
 * Game Inputs:
 * - Mix it! -> Rotary potentiometer (turn the knob)
 * - Cut it! -> Slide potentiometer (slide the slider)
 * - Cook it! -> Push button (press the button)
 * 
 */

#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"

#if (defined(ARDUINO_AVR_UNO) || defined(ESP8266))   // Using a soft serial port
  #include <SoftwareSerial.h>
  SoftwareSerial softSerial(/*rx =*/9, /*tx =*/10);
  #define FPSerial softSerial
#else
  #define FPSerial Serial1
#endif

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= OLED CONFIG =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= INPUTS / OUTPUTS =================
// Potentiometers
#define ROTARY_POT_PIN  A3
#define LINEAR_POT_PIN  A2

// Score button (increments score)
#define BUTTON_PIN 2   // D2 / PD2

// START button (toggle start/pause/resume)
#define START_BUTTON_PIN 4   // D4 / PD4 / physical pin 6

// LEDs
#define LED1 8
#define LED2 7

// Thresholds
#define ROTARY_THRESHOLD 300
#define LINEAR_THRESHOLD 200

// Debounce
#define DEBOUNCE_DELAY 20

// ================= GAME CONFIG =================
#define INITIAL_TIME_LIMIT 10000   // 10 seconds initially
#define MIN_TIME_LIMIT 3000        // Minimum 3 seconds
#define INTRO_DURATION 4000        // Wait 4 seconds for intro to play

// Audio track numbers
#define TRACK_INTRO 1
#define TRACK_CUT_IT 2    // Linear pot
#define TRACK_MIX_IT 3    // Rotary pot
#define TRACK_BAKE_IT 4   // Button
#define TRACK_YOU_LOSE 5

// Challenge types
enum ChallengeType {
  CHALLENGE_BUTTON = 0,
  CHALLENGE_ROTARY = 1,
  CHALLENGE_LINEAR = 2,
  NUM_CHALLENGES = 3
};

// ================= STATE =================
// Game state
enum GameState {
  STATE_INTRO,        // Playing intro sound (before START)
  STATE_WAITING,      // Waiting for START button (after intro)
  STATE_NEW_CHALLENGE,// Setting up new challenge
  STATE_PLAYING,      // Waiting for user input
  STATE_GAME_OVER     // Lost - waiting for restart
};

GameState gameState = STATE_INTRO;

// Challenge state
ChallengeType currentChallenge;
unsigned long challengeStartTime = 0;
unsigned long currentTimeLimit = INITIAL_TIME_LIMIT;

// Intro timer
unsigned long introStartTime = 0;
bool introFinished = false;

// Edge-detect flags for pots
bool rotaryTriggered = false;
bool linearTriggered = false;

// Score button debounce state
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// START button debounce state
int startButtonState = HIGH;
int lastStartButtonState = HIGH;
unsigned long lastStartDebounceTime = 0;

// Score
int score = 0;

// Input detected flags
bool inputDetected = false;
bool correctInput = false;

// Audio playing delay
unsigned long audioDelay = 0;
const unsigned long AUDIO_WAIT_TIME = 1500; // Wait 1.5s after audio starts

// ================= DFPLAYER =================
DFRobotDFPlayerMini myDFPlayer;

// ================= OLED FUNCTIONS =================
void displayScore() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Score:"));

  display.setTextSize(2);
  display.setCursor(0, 12);
  display.print(score);

  display.display();
}

void displayPressStart() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Press START"));
  display.println(F("to begin"));
  display.display();
}

void displayChallenge(const char* text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(text);
  
  // Show score on second line
  display.print(F("Score: "));
  display.println(score);
  
  // Show time remaining
  unsigned long elapsed = millis() - challengeStartTime;
  unsigned long remaining = 0;
  if (currentTimeLimit > elapsed) {
    remaining = (currentTimeLimit - elapsed) / 1000;
  }
  display.print(F("Time: "));
  display.print(remaining + 1);
  display.println(F("s"));
  
  display.display();
}

void displayGameOver() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("GAME OVER!"));
  display.print(F("Final Score: "));
  display.println(score);
  display.println(F("Press START"));
  display.display();
}

void incrementScore() {
  score++;
  Serial.print(F("Score: "));
  Serial.println(score);
}

// ================= GAME FUNCTIONS =================

unsigned long calculateTimeLimit(int currentScore) {
  // Progressive speed-up: 10s -> 9s -> 7s -> 5s -> 3s (min)
  if (currentScore == 0) return 10000;      // First: 10 seconds
  else if (currentScore == 1) return 9000;  // Second: 9 seconds
  else if (currentScore == 2) return 7000;  // Third: 7 seconds
  else if (currentScore == 3) return 5000;  // Fourth: 5 seconds
  else return 3000;                         // Fifth onwards: 3 seconds minimum
}

void startNewChallenge() {
  // Pick random challenge
  currentChallenge = (ChallengeType)random(NUM_CHALLENGES);
  
  // Reset flags
  rotaryTriggered = false;
  linearTriggered = false;
  inputDetected = false;
  correctInput = false;
  
  // Calculate new time limit based on score
  currentTimeLimit = calculateTimeLimit(score);
  
  // Play appropriate sound
  switch(currentChallenge) {
    case CHALLENGE_BUTTON:
      myDFPlayer.play(TRACK_BAKE_IT);
      Serial.println(F("Challenge: BAKE IT (Button)"));
      break;
    case CHALLENGE_ROTARY:
      myDFPlayer.play(TRACK_MIX_IT);
      Serial.println(F("Challenge: MIX IT (Rotary)"));
      break;
    case CHALLENGE_LINEAR:
      myDFPlayer.play(TRACK_CUT_IT);
      Serial.println(F("Challenge: CUT IT (Linear)"));
      break;
  }
  
  audioDelay = millis();
  gameState = STATE_NEW_CHALLENGE;
}

void startChallengeTiming() {
  challengeStartTime = millis();
  gameState = STATE_PLAYING;
  Serial.print(F("Time limit: "));
  Serial.print(currentTimeLimit);
  Serial.println(F("ms"));
}

void checkTimeout() {
  if (millis() - challengeStartTime > currentTimeLimit) {
    // Time's up!
    Serial.println(F("Time's up! Game Over."));
    myDFPlayer.play(TRACK_YOU_LOSE);
    gameState = STATE_GAME_OVER;
    displayGameOver();
  }
}

void handleCorrectInput() {
  incrementScore();
  
  // Flash both LEDs
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
  delay(100);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  
  // Small delay before next challenge
  delay(500);
  
  // Start new challenge
  startNewChallenge();
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Booting Bop-It Game..."));

  // Random seed from analog pin
  randomSeed(analogRead(A0));

  // LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  // Pots
  pinMode(ROTARY_POT_PIN, INPUT);
  pinMode(LINEAR_POT_PIN, INPUT);

  // Buttons
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);

  // I2C + OLED init
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  displayPressStart();

  // DFPlayer serial + init
#if (defined ESP32)
  FPSerial.begin(9600, SERIAL_8N1, /*rx=*/D3, /*tx=*/D2);
#else
  FPSerial.begin(9600);
#endif

  Serial.println(F("Initializing DFPlayer..."));
  if (!myDFPlayer.begin(FPSerial, /*isACK=*/true, /*doReset=*/true)) {
    Serial.println(F("DFPlayer init failed. Check wiring + SD card."));
    for(;;) { delay(0); }
  }

  delay(500); // Wait for DFPlayer to stabilize
  myDFPlayer.volume(20);  // 0–30
  
  // Play intro immediately on boot
  Serial.println(F("Playing intro..."));
  myDFPlayer.play(TRACK_INTRO);
  introStartTime = millis();
  introFinished = false;
  gameState = STATE_INTRO;
}

// ================= LOOP =================

void loop() {
  // Check if intro has finished
  if (gameState == STATE_INTRO && !introFinished) {
    if (millis() - introStartTime >= INTRO_DURATION) {
      introFinished = true;
      gameState = STATE_WAITING;
      Serial.println(F("Intro finished. Press START to begin!"));
    }
    return; // Don't process anything else during intro
  }

  // Always check START button (but only after intro)
  if (introFinished) {
    handleStartButton();
  }

  // State machine
  switch(gameState) {
    case STATE_INTRO:
      // Handled above
      break;
      
    case STATE_WAITING:
      // Just waiting for START button
      break;
      
    case STATE_NEW_CHALLENGE:
      // Wait for audio to play
      if (millis() - audioDelay > AUDIO_WAIT_TIME) {
        startChallengeTiming();
      }
      // Update display during audio
      switch(currentChallenge) {
        case CHALLENGE_BUTTON:
          displayChallenge("BAKE IT!");
          break;
        case CHALLENGE_ROTARY:
          displayChallenge("MIX IT!");
          break;
        case CHALLENGE_LINEAR:
          displayChallenge("CUT IT!");
          break;
      }
      break;
      
    case STATE_PLAYING:
      // Check for timeout
      checkTimeout();
      
      // Check for inputs
      if (!inputDetected) {
        handleScoreButton();
        handleRotaryPot();
        handleLinearPot();
        
        if (inputDetected && correctInput) {
          handleCorrectInput();
        } else if (inputDetected && !correctInput) {
          // Wrong input!
          Serial.println(F("Wrong input! Game Over."));
          myDFPlayer.play(TRACK_YOU_LOSE);
          gameState = STATE_GAME_OVER;
          displayGameOver();
        }
      }
      
      // Update display
      switch(currentChallenge) {
        case CHALLENGE_BUTTON:
          displayChallenge("BAKE IT!");
          break;
        case CHALLENGE_ROTARY:
          displayChallenge("MIX IT!");
          break;
        case CHALLENGE_LINEAR:
          displayChallenge("CUT IT!");
          break;
      }
      break;
      
    case STATE_GAME_OVER:
      // Just waiting for START button to restart
      break;
  }
}

// ================= INPUT HANDLERS =================

/**
 * START button handler (debounced)
 */
void handleStartButton() {
  int reading = digitalRead(START_BUTTON_PIN);

  if (reading != lastStartButtonState) {
    lastStartDebounceTime = millis();
  }

  if ((millis() - lastStartDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != startButtonState) {
      startButtonState = reading;

      if (startButtonState == LOW) {
        if (gameState == STATE_WAITING || gameState == STATE_GAME_OVER) {
          // Start new game
          Serial.println(F("START pressed — New game!"));
          
          score = 0;
          currentTimeLimit = INITIAL_TIME_LIMIT;
          rotaryTriggered = false;
          linearTriggered = false;
          
          displayScore();
          
          // Start first challenge
          startNewChallenge();
        }
      }
    }
  }

  lastStartButtonState = reading;
}

/**
 * Score button press with debouncing
 */
void handleScoreButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        inputDetected = true;
        correctInput = (currentChallenge == CHALLENGE_BUTTON);
        
        if (correctInput) {
          Serial.println(F("Button pressed - CORRECT!"));
        } else {
          Serial.println(F("Button pressed - WRONG!"));
        }
      }
    }
  }

  lastButtonState = reading;
}

/**
 * Rotary potentiometer input (edge-triggered)
 */
void handleRotaryPot() {
  int rotaryValue = analogRead(ROTARY_POT_PIN);

  if (rotaryValue > ROTARY_THRESHOLD && !rotaryTriggered) {
    rotaryTriggered = true;
    inputDetected = true;
    correctInput = (currentChallenge == CHALLENGE_ROTARY);

    digitalWrite(LED1, HIGH);
    delay(50);
    digitalWrite(LED1, LOW);

    if (correctInput) {
      Serial.println(F("Rotary pot - CORRECT!"));
    } else {
      Serial.println(F("Rotary pot - WRONG!"));
    }
    Serial.print(F("  Rotary value: "));
    Serial.println(rotaryValue);
  }
  else if (rotaryValue <= ROTARY_THRESHOLD) {
    rotaryTriggered = false;
  }
}

/**
 * Linear potentiometer input (edge-triggered)
 */
void handleLinearPot() {
  int linearValue = analogRead(LINEAR_POT_PIN);

  if (linearValue > LINEAR_THRESHOLD && !linearTriggered) {
    linearTriggered = true;
    inputDetected = true;
    correctInput = (currentChallenge == CHALLENGE_LINEAR);

    digitalWrite(LED2, HIGH);
    delay(50);
    digitalWrite(LED2, LOW);

    if (correctInput) {
      Serial.println(F("Linear pot - CORRECT!"));
    } else {
      Serial.println(F("Linear pot - WRONG!"));
    }
    Serial.print(F("  Linear value: "));
    Serial.println(linearValue);
  }
  else if (linearValue <= LINEAR_THRESHOLD) {
    linearTriggered = false;
  }
}