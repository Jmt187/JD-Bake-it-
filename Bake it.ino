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

// ================= STATE =================
// Game state
bool gameRunning = false;     // true = inputs active
bool gameInitialized = false; // becomes true after the first START press

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

void displayPaused() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Paused"));
  display.println(F("Press START"));
  display.display();
}

// Centralized score increment
void incrementScore(const __FlashStringHelper* source) {
  score++;
  displayScore();

  Serial.print(source);
  Serial.print(F(" Score: "));
  Serial.println(score);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Booting Start-toggle + Button + Pots + LEDs + OLED + DFPlayer..."));

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

  myDFPlayer.volume(20);  // 0–30

  // ✅ Play ONLY 0001.mp3 (track 1) immediately on boot (before START)
  myDFPlayer.play(1);
  Serial.println(F("Boot audio: playing track 1 (0001.mp3)."));

  Serial.println(F("Waiting for START button..."));
}

void loop() {
  // Always check START button
  handleStartButton();

  // If game is paused/not started, ignore scoring inputs
  if (!gameRunning) return;

  // Game inputs
  handleScoreButton();
  handleRotaryPot();
  handleLinearPot();

  // ✅ No auto-next / no other track logic
}

/**
 * START button handler (debounced)
 * 1st press: start game (reset score)
 * later presses: toggle pause/resume
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
        // FIRST TIME: initialize and start
        if (!gameInitialized) {
          gameInitialized = true;
          gameRunning = true;

          score = 0;
          rotaryTriggered = false;
          linearTriggered = false;

          displayScore();

          // ✅ Do NOT start any track here (boot already started 0001.mp3)
          Serial.println(F("START pressed — game begun!"));
        }
        // TOGGLE: pause/resume
        else {
          gameRunning = !gameRunning;

          if (!gameRunning) {
            myDFPlayer.pause();
            displayPaused();
            Serial.println(F("Game paused."));
          } else {
            myDFPlayer.start();   // resume current (still only track 1)
            displayScore();
            Serial.println(F("Game resumed."));
          }
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
        incrementScore(F("Score button pressed!"));
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

    digitalWrite(LED1, HIGH);
    delay(100);
    digitalWrite(LED1, LOW);

    incrementScore(F("Rotary pot triggered!"));
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

    digitalWrite(LED2, HIGH);
    delay(100);
    digitalWrite(LED2, LOW);

    incrementScore(F("Linear pot triggered!"));
    Serial.print(F("  Linear value: "));
    Serial.println(linearValue);
  }
  else if (linearValue <= LINEAR_THRESHOLD) {
    linearTriggered = false;
  }
}
