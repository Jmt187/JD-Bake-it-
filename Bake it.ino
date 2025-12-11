/*
 * BAKE IT! - Junior Design Project
 * A Bop-it style game with baking theme for ATmega328P
 *
 * Audio files:
 * 0001.mp3 = intro ("welcome to bake it!")  [played once on boot]
 * 0002.mp3 = "cut it!"
 * 0003.mp3 = "mix it!"
 * 0004.mp3 = "cook it!"
 * 0005.mp3 = lose sound
 *
 * Inputs:
 * - CUT IT  -> Linear potentiometer (A2)  [movement-based trigger]
 * - MIX IT  -> Rotary potentiometer (A3)  [movement-based trigger]
 * - COOK IT -> Push button (D2)           [press trigger]
 *
 * START button (D4):
 * - First press starts the run
 * - Later presses pause/resume
 */

#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"

#if (defined(ARDUINO_AVR_UNO) || defined(ESP8266))
  #include <SoftwareSerial.h>
  SoftwareSerial softSerial(/*rx=*/9, /*tx=*/10);
  #define FPSerial softSerial
#else
  #define FPSerial Serial1
#endif

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= OLED CONFIG =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= PINS =================
#define ROTARY_POT_PIN  A3
#define LINEAR_POT_PIN  A2

#define COOK_BUTTON_PIN 2      // D2 / PD2
#define START_BUTTON_PIN 4     // D4 / PD4 / physical pin 6

// ================= DFPLAYER =================
DFRobotDFPlayerMini myDFPlayer;

// ===== Timing fixes =====
const unsigned long DFPLAYER_READY_DELAY_MS = 1200;
const unsigned long INTRO_DELAY_MS          = 3000;

// ================= GAME TIMING (like reference) =================
int timeDelay = 3000;                 // start at 3.0 seconds
const int MIN_TIME_DELAY = 500;       // min 0.5 seconds
const int SPEEDUP_STEP = 200;         // -200ms every 5 rounds
const int SPEEDUP_EVERY = 5;

const unsigned long BETWEEN_ROUNDS_MS = 400;  // small breather between prompts

// Pot movement sensitivity (tune if needed)
const int POT_CHANGE = 120;

// ================= STATE =================
bool gameRunning = false;
bool gameInitialized = false;

// Start button debounce
const unsigned long DEBOUNCE_DELAY = 20;
int startButtonState = HIGH;
int lastStartButtonState = HIGH;
unsigned long lastStartDebounceTime = 0;

// Run stats
int score = 0;        // equals rounds passed
int roundsPassed = 0;

// Round state
bool roundActive = false;
uint8_t currentPromptTrack = 0;   // 2,3,4
unsigned long roundDeadline = 0;
unsigned long nextRoundAt = 0;

// Pause behavior
bool resumeSamePrompt = false;
uint8_t pausedPromptTrack = 0;

// Input tracking (movement/edge detection)
int cookLastState = HIGH;
int rotaryLastValue = 0;
int linearLastValue = 0;

// ================= OLED HELPERS =================
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

void displayLose() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("You lose!"));
  display.println(F("Press START"));
  display.display();
}

void displayScoreOnly() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("Score: "));
  display.println(score);

  display.setTextSize(1);
  display.setCursor(0, 16);
  display.print(F("Time: "));
  display.print(timeDelay);
  display.println(F("ms"));

  display.display();
}

void displayPrompt(uint8_t track) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // top line: score
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Score: "));
  display.println(score);

  // big prompt
  display.setTextSize(2);
  display.setCursor(0, 14);
  if (track == 2) display.print(F("CUT!"));
  else if (track == 3) display.print(F("MIX!"));
  else if (track == 4) display.print(F("COOK!"));
  else display.print(F("..."));

  display.display();
}

// ================= INPUT SYNC =================
void syncInputsForRound() {
  cookLastState = digitalRead(COOK_BUTTON_PIN);
  rotaryLastValue = analogRead(ROTARY_POT_PIN);
  linearLastValue = analogRead(LINEAR_POT_PIN);
}

// ================= INPUT CHECKERS =================
bool cookPressed() {
  int cur = digitalRead(COOK_BUTTON_PIN);
  bool pressed = (cookLastState == HIGH && cur == LOW);
  cookLastState = cur;
  return pressed;
}

bool rotaryTurned() {
  int cur = analogRead(ROTARY_POT_PIN);
  if (abs(cur - rotaryLastValue) > POT_CHANGE) {
    rotaryLastValue = cur;
    return true;
  }
  return false;
}

bool linearSlid() {
  int cur = analogRead(LINEAR_POT_PIN);
  if (abs(cur - linearLastValue) > POT_CHANGE) {
    linearLastValue = cur;
    return true;
  }
  return false;
}

bool correctInputForPrompt(uint8_t promptTrack) {
  switch (promptTrack) {
    case 2: return linearSlid();   // CUT
    case 3: return rotaryTurned(); // MIX
    case 4: return cookPressed();  // COOK
    default: return false;
  }
}

// ================= ROUND CONTROL =================
void startRound(bool replaySame = false) {
  if (!replaySame) {
    currentPromptTrack = (uint8_t)random(2, 5); // 2..4
  }
  syncInputsForRound();
  displayPrompt(currentPromptTrack);

  // Play the prompt (0002/0003/0004)
  myDFPlayer.play(currentPromptTrack);

  roundDeadline = millis() + (unsigned long)timeDelay;
  roundActive = true;

  Serial.print(F("Prompt track: "));
  Serial.print(currentPromptTrack);
  Serial.print(F("  timeDelay="));
  Serial.println(timeDelay);
}

void handleCorrect() {
  myDFPlayer.stop();  // stop prompt audio immediately

  score++;
  roundsPassed++;

  // Speed up every 5 rounds
  if (roundsPassed > 0 && (roundsPassed % SPEEDUP_EVERY == 0)) {
    if (timeDelay > MIN_TIME_DELAY) {
      timeDelay -= SPEEDUP_STEP;
      if (timeDelay < MIN_TIME_DELAY) timeDelay = MIN_TIME_DELAY;
    }
  }

  displayScoreOnly();

  roundActive = false;
  nextRoundAt = millis() + BETWEEN_ROUNDS_MS;
}

void handleLose() {
  myDFPlayer.stop();
  myDFPlayer.play(5);   // 0005.mp3 lose
  displayLose();

  // Let the lose audio get a head start so it isn't cut off
  delay(2000);

  // Reset the run
  timeDelay = 3000;
  roundsPassed = 0;
  score = 0;

  roundActive = false;
  currentPromptTrack = 0;

  gameRunning = false;
  gameInitialized = false;

  // Wait for START again
  displayPressStart();
}

// ================= START BUTTON =================
bool handleStartButton() {
  int reading = digitalRead(START_BUTTON_PIN);

  if (reading != lastStartButtonState) {
    lastStartDebounceTime = millis();
  }

  bool handledEvent = false;

  if ((millis() - lastStartDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != startButtonState) {
      startButtonState = reading;

      if (startButtonState == LOW) {
        handledEvent = true;

        // First time: start fresh run
        if (!gameInitialized) {
          gameInitialized = true;
          gameRunning = true;

          timeDelay = 3000;
          roundsPassed = 0;
          score = 0;

          roundActive = false;
          currentPromptTrack = 0;

          displayScoreOnly();
          nextRoundAt = millis();

          Serial.println(F("START: run begun."));
        }
        // Toggle pause/resume
        else {
          gameRunning = !gameRunning;

          if (!gameRunning) {
            // Pausing
            myDFPlayer.pause();
            displayPaused();

            // If we were mid-round, remember the prompt and replay it on resume
            if (roundActive && currentPromptTrack >= 2 && currentPromptTrack <= 4) {
              resumeSamePrompt = true;
              pausedPromptTrack = currentPromptTrack;
              roundActive = false;  // prevent timeout while paused
            } else {
              resumeSamePrompt = false;
              pausedPromptTrack = 0;
            }

            Serial.println(F("Paused."));
          } else {
            // Resuming
            // We'll replay the same prompt (fresh timer) if we paused mid-round
            if (resumeSamePrompt && pausedPromptTrack >= 2 && pausedPromptTrack <= 4) {
              currentPromptTrack = pausedPromptTrack;
              resumeSamePrompt = false;
              pausedPromptTrack = 0;

              startRound(true); // replay same prompt, reset timer
            } else {
              nextRoundAt = millis(); // start a new round
            }

            Serial.println(F("Resumed."));
          }
        }
      }
    }
  }

  lastStartButtonState = reading;
  return handledEvent;
}

// ================= SETUP / LOOP =================
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Booting BAKE IT (round timer + random prompts)..."));

  pinMode(COOK_BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  displayPressStart();

  // DFPlayer init
  FPSerial.begin(9600);

  Serial.println(F("Initializing DFPlayer..."));
  if (!myDFPlayer.begin(FPSerial, /*isACK=*/true, /*doReset=*/true)) {
    Serial.println(F("DFPlayer init failed. Check wiring + SD card."));
    for(;;) { delay(0); }
  }

  myDFPlayer.volume(15);

  delay(DFPLAYER_READY_DELAY_MS);

  // Play intro once
  myDFPlayer.play(1); // 0001.mp3
  Serial.println(F("Intro: playing track 1 (0001.mp3)."));
  delay(INTRO_DELAY_MS);

  // Seed RNG (use a floating analog pin if possible)
  randomSeed(analogRead(A1));

  Serial.println(F("Waiting for START..."));
}

void loop() {
  // Always allow START to be responsive
  if (handleStartButton()) return;

  if (!gameRunning) return;

  unsigned long now = millis();

  // If no active round, start one when allowed
  if (!roundActive) {
    if (now >= nextRoundAt) {
      startRound(false);
    }
    return;
  }

  // Check correct input during the time window
  if (correctInputForPrompt(currentPromptTrack)) {
    handleCorrect();
    return;
  }

  // Timeout = lose
  if ((long)(now - roundDeadline) >= 0) {
    handleLose();
    return;
  }
}