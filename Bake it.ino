/*
 * BAKE IT! - Junior Design Project (robust DFPlayer + anti-ghost scoring)
 *
 * SD card layout (recommended):
 *   /MP3/0001.mp3  intro
 *   /MP3/0002.mp3  cut it
 *   /MP3/0003.mp3  mix it
 *   /MP3/0004.mp3  bake it
 *   /MP3/0005.mp3  lose
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
#define ROTARY_POT_PIN   A3
#define LINEAR_POT_PIN   A2
#define COOK_BUTTON_PIN  2
#define START_BUTTON_PIN 4

// ================= DFPLAYER =================
DFRobotDFPlayerMini myDFPlayer;

// DFPlayer stability tuning
const unsigned long DFPLAYER_READY_DELAY_MS = 1200;
const unsigned long DF_CMD_GAP_MS           = 140;  // min gap between DF commands
unsigned long dfLastCmdMs = 0;
bool dfPlaying = false;

// Intro / lose timing
const unsigned long INTRO_DELAY_MS = 3000;

// ================= GAME TIMING =================
int timeDelay = 3000;
const int MIN_TIME_DELAY = 500;
const int SPEEDUP_STEP = 200;
const int SPEEDUP_EVERY = 5;

const unsigned long BETWEEN_ROUNDS_MS = 400;

// Pot movement sensitivity
const int POT_CHANGE = 120;

// Prevent “instant win” that cuts/skips audio
const unsigned long PROMPT_LOCKOUT_MS = 250; // ignore inputs briefly after prompt starts

// ================= STATE =================
bool gameRunning = false;
bool gameInitialized = false;

// Start button debounce
const unsigned long DEBOUNCE_DELAY = 20;
int startButtonState = HIGH;
int lastStartButtonState = HIGH;
unsigned long lastStartDebounceTime = 0;

// Run stats
int score = 0;
int roundsPassed = 0;

// Round state
bool roundActive = false;
uint8_t currentPromptTrack = 0;   // 2,3,4
unsigned long promptStartMs = 0;
unsigned long roundDeadline = 0;
unsigned long nextRoundAt = 0;

// Pause behavior
bool resumeSamePrompt = false;
uint8_t pausedPromptTrack = 0;

// Input tracking
int cookLastState = HIGH;
int rotaryLastValue = 0;
int linearLastValue = 0;

// ================= DFPLAYER SERVICE =================
void serviceDFPlayer() {
  // Drain any messages so SoftwareSerial RX buffer doesn't fill over time.
  while (myDFPlayer.available()) {
    (void)myDFPlayer.readType();
    (void)myDFPlayer.read();
  }
}

void dfWaitGap() {
  // Wait until we respect DF_CMD_GAP_MS between commands, but keep draining DFPlayer.
  while (millis() - dfLastCmdMs < DF_CMD_GAP_MS) {
    serviceDFPlayer();
    delay(1);
  }
}

void dfStop() {
  if (!dfPlaying) return;
  dfWaitGap();
  myDFPlayer.stop();
  dfLastCmdMs = millis();
  dfPlaying = false;
}

void dfPlayMp3(uint16_t n) {
  // Only stop if we think something is currently playing
  dfStop();
  dfWaitGap();
  myDFPlayer.playMp3Folder(n); // /MP3/000n.mp3
  dfLastCmdMs = millis();
  dfPlaying = true;
}

// “smart” delay: keeps DFPlayer RX drained during long waits
void smartDelay(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    serviceDFPlayer();
    delay(1);
  }
}

bool initDFPlayer() {
#if (defined(ARDUINO_AVR_UNO) || defined(ESP8266))
  softSerial.listen(); // ensure SoftwareSerial is active after reset
#endif

  for (int attempt = 1; attempt <= 3; attempt++) {
    // isACK = false reduces serial chatter; doReset = true resyncs DFPlayer
    if (myDFPlayer.begin(FPSerial, /*isACK=*/false, /*doReset=*/true)) {
      delay(DFPLAYER_READY_DELAY_MS);
      myDFPlayer.volume(15);
      dfLastCmdMs = millis();
      dfPlaying = false;
      serviceDFPlayer();
      return true;
    }
    delay(800);
  }
  return false;
}

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

void displayDFError() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("DFPlayer FAIL"));
  display.println(F("Check SD/wiring"));
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

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Score: "));
  display.println(score);

  display.setTextSize(2);
  display.setCursor(0, 14);
  if (track == 2) display.print(F("CUT!"));
  else if (track == 3) display.print(F("MIX!"));
  else if (track == 4) display.print(F("BAKE!"));
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
    case 4: return cookPressed();  // BAKE input is the button
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

  // Start audio prompt (0002/0003/0004)
  dfPlayMp3(currentPromptTrack);

  promptStartMs = millis();

  // Keep the *effective* reaction time the same by extending deadline
  // by the same lockout used to prevent instant ghost scoring.
  roundDeadline = promptStartMs + (unsigned long)timeDelay + PROMPT_LOCKOUT_MS;

  roundActive = true;
}

void handleCorrect() {
  // Don’t cut audio instantly before lockout expires
  dfStop();

  score++;
  roundsPassed++;

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
  dfStop();
  dfPlayMp3(5); // lose sound
  displayLose();

  smartDelay(2000);

  // Reset run
  timeDelay = 3000;
  roundsPassed = 0;
  score = 0;

  roundActive = false;
  currentPromptTrack = 0;

  gameRunning = false;
  gameInitialized = false;

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
        } else {
          gameRunning = !gameRunning;

          if (!gameRunning) {
            // Stop audio cleanly on pause (more reliable than DF pause on many modules)
            dfStop();
            displayPaused();

            if (roundActive && currentPromptTrack >= 2 && currentPromptTrack <= 4) {
              resumeSamePrompt = true;
              pausedPromptTrack = currentPromptTrack;
              roundActive = false; // freeze round while paused
            } else {
              resumeSamePrompt = false;
              pausedPromptTrack = 0;
            }
          } else {
            // Resume: replay prompt with a fresh timer (consistent and reliable)
            if (resumeSamePrompt && pausedPromptTrack >= 2 && pausedPromptTrack <= 4) {
              currentPromptTrack = pausedPromptTrack;
              resumeSamePrompt = false;
              pausedPromptTrack = 0;
              startRound(true);
            } else {
              nextRoundAt = millis();
            }
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

  pinMode(COOK_BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;);
  }

  displayPressStart();

  FPSerial.begin(9600);

  if (!initDFPlayer()) {
    displayDFError();
    // Hard stop so you immediately know audio isn't initialized
    for(;;) { delay(100); }
  }

  // Intro
  dfPlayMp3(1);
  smartDelay(INTRO_DELAY_MS);

  randomSeed(analogRead(A1));
}

void loop() {
  serviceDFPlayer(); // continuously drain DFPlayer messages

  if (handleStartButton()) return;
  if (!gameRunning) return;

  unsigned long now = millis();

  if (!roundActive) {
    if (now >= nextRoundAt) {
      startRound(false);
    }
    return;
  }

  // Don’t accept inputs immediately after prompt starts
  if (now - promptStartMs < PROMPT_LOCKOUT_MS) return;

  if (correctInputForPrompt(currentPromptTrack)) {
    handleCorrect();
    return;
  }

  if ((long)(now - roundDeadline) >= 0) {
    handleLose();
    return;
  }
}
