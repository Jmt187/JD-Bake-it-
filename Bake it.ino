/*
 * BAKE IT! - Junior Design Project
 * A Bop-it style game with baking theme for ATmega328P
 * 
 * Game Inputs:
 * - Mix it! -> Rotary potentiometer (turn the knob)
 * - Cut it! -> Slide potentiometer (slide the slider)
 * - Cook it! -> Push button (press the button)
 * 
 * Hardware: ATmega328P + DFPlayer Mini + Speaker + Potentiometers + Buttons
 */

#include <SoftwareSerial.h>

// ========== PIN DEFINITIONS ==========
const int ROTARY_POT_PIN = A0;       // Mix it! (Analog 0 / ATmega Pin 23)
const int SLIDE_POT_PIN = A1;        // Cut it! (Analog 1 / ATmega Pin 24)
const int COOK_BUTTON_PIN = 2;       // Cook it! (Digital 2 / ATmega Pin 4)
const int START_BUTTON_PIN = 3;      // Start game (Digital 3 / ATmega Pin 5)
const int DFPLAYER_RX = 10;          // To DFPlayer TX (ATmega Pin 16)
const int DFPLAYER_TX = 11;          // To DFPlayer RX (ATmega Pin 17)
const int STATUS_LED_PIN = 8;        // Status LED (ATmega Pin 14)

// ========== GAME CONSTANTS ==========
const int MAX_SCORE = 99;                    // Win at 99 points
const int INITIAL_TIME_LIMIT = 3000;         // Start with 3 seconds
const int MIN_TIME_LIMIT = 800;              // Fastest: 0.8 seconds
const int TIME_DECREASE_INTERVAL = 5;        // Speed up every 5 points
const int TIME_DECREASE_AMOUNT = 200;        // Decrease by 200ms each time
const int POT_THRESHOLD = 300;               // Pot movement threshold

// ========== MP3 FILE NUMBERS (on SD card) ==========
const int MP3_WELCOME = 1;        // 0001.mp3 - "Welcome to Bake It!"
const int MP3_MIX_IT = 2;         // 0002.mp3 - "Mix it!"
const int MP3_CUT_IT = 3;         // 0003.mp3 - "Cut it!"
const int MP3_COOK_IT = 4;        // 0004.mp3 - "Cook it!"
const int MP3_CORRECT = 5;        // 0005.mp3 - Success sound
const int MP3_WRONG = 6;          // 0006.mp3 - Failure sound
const int MP3_GAME_OVER = 7;      // 0007.mp3 - Game over
const int MP3_HIGH_SCORE = 8;     // 0008.mp3 - Victory message

// ========== ENUMS ==========
enum GameState { IDLE, PLAYING, GAME_OVER };
enum InputType { MIX_IT = 0, CUT_IT = 1, COOK_IT = 2 };

// ========== GLOBAL VARIABLES ==========
SoftwareSerial dfPlayerSerial(DFPLAYER_RX, DFPLAYER_TX);
GameState gameState = IDLE;
int currentScore = 0;
int currentTimeLimit = INITIAL_TIME_LIMIT;
InputType currentPrompt;
unsigned long promptStartTime = 0;
int rotaryBaseline = 0;
int slideBaseline = 0;
unsigned long lastButtonPressTime = 0;
const int DEBOUNCE_DELAY = 50;

// ========== SETUP ==========
void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  dfPlayerSerial.begin(9600);
  
  // Configure pins
  pinMode(COOK_BUTTON_PIN, INPUT_PULLUP);     // Button with internal pullup
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);    // Button with internal pullup
  pinMode(STATUS_LED_PIN, OUTPUT);
  
  // Initialize DFPlayer Mini
  delay(1000);                              // Wait for DFPlayer to boot
  sendDFPlayerCommand(0x09, 0, 2);          // Select SD card
  delay(200);
  sendDFPlayerCommand(0x06, 0, 20);         // Set volume (0-30)
  delay(200);
  
  // Blink LED 3 times to show system ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(200);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(200);
  }
  
  Serial.println("==================================");
  Serial.println("Bake It! Ready");
  Serial.println("Press START button to begin!");
  Serial.println("==================================");
}

// ========== MAIN LOOP ==========
void loop() {
  switch (gameState) {
    case IDLE:
      // Wait for start button
      if (digitalRead(START_BUTTON_PIN) == LOW && 
          millis() - lastButtonPressTime > DEBOUNCE_DELAY) {
        lastButtonPressTime = millis();
        startGame();
      }
      break;
      
    case PLAYING:
      playGameRound();
      break;
      
    case GAME_OVER:
      handleGameOver();
      break;
  }
}

// ========== GAME FUNCTIONS ==========

void startGame() {
  Serial.println("\n*** GAME STARTING ***");
  
  // Reset game variables
  currentScore = 0;
  currentTimeLimit = INITIAL_TIME_LIMIT;
  gameState = PLAYING;
  
  // Play welcome message
  playMP3(MP3_WELCOME);
  delay(2000);  // Wait for welcome to finish
  
  // Take baseline readings for potentiometers
  rotaryBaseline = analogRead(ROTARY_POT_PIN);
  slideBaseline = analogRead(SLIDE_POT_PIN);
  
  Serial.print("Rotary baseline: ");
  Serial.println(rotaryBaseline);
  Serial.print("Slide baseline: ");
  Serial.println(slideBaseline);
  
  // Start first round
  issueNewPrompt();
}

void playGameRound() {
  // Check if time expired
  if (millis() - promptStartTime > currentTimeLimit) {
    Serial.println("TIME'S UP!");
    handleWrongAnswer();
    return;
  }
  
  // Check for correct input
  bool correctInput = false;
  
  switch (currentPrompt) {
    case MIX_IT:
      correctInput = checkRotaryPot();
      break;
    case CUT_IT:
      correctInput = checkSlidePot();
      break;
    case COOK_IT:
      correctInput = checkButton();
      break;
  }
  
  if (correctInput) {
    handleCorrectAnswer();
  }
  
  delay(10);  // Small delay to prevent excessive polling
}

void issueNewPrompt() {
  // Randomly select prompt
  currentPrompt = (InputType)random(0, 3);
  
  // Update baseline readings
  rotaryBaseline = analogRead(ROTARY_POT_PIN);
  slideBaseline = analogRead(SLIDE_POT_PIN);
  
  // Play prompt audio and print to serial
  Serial.print("\n>>> ");
  switch (currentPrompt) {
    case MIX_IT:
      Serial.print("MIX IT!");
      playMP3(MP3_MIX_IT);
      break;
    case CUT_IT:
      Serial.print("CUT IT!");
      playMP3(MP3_CUT_IT);
      break;
    case COOK_IT:
      Serial.print("COOK IT!");
      playMP3(MP3_COOK_IT);
      break;
  }
  Serial.print(" (Time: ");
  Serial.print(currentTimeLimit);
  Serial.println("ms)");
  
  // Start timer
  promptStartTime = millis();
  
  // Quick LED blink to indicate new prompt
  digitalWrite(STATUS_LED_PIN, HIGH);
  delay(100);
  digitalWrite(STATUS_LED_PIN, LOW);
}

bool checkRotaryPot() {
  int currentValue = analogRead(ROTARY_POT_PIN);
  int change = abs(currentValue - rotaryBaseline);
  
  if (change > POT_THRESHOLD) {
    Serial.print("Rotary pot moved: ");
    Serial.print(change);
    Serial.print(" (baseline: ");
    Serial.print(rotaryBaseline);
    Serial.print(", current: ");
    Serial.print(currentValue);
    Serial.println(")");
    return true;
  }
  return false;
}

bool checkSlidePot() {
  int currentValue = analogRead(SLIDE_POT_PIN);
  int change = abs(currentValue - slideBaseline);
  
  if (change > POT_THRESHOLD) {
    Serial.print("Slide pot moved: ");
    Serial.print(change);
    Serial.print(" (baseline: ");
    Serial.print(slideBaseline);
    Serial.print(", current: ");
    Serial.print(currentValue);
    Serial.println(")");
    return true;
  }
  return false;
}

bool checkButton() {
  if (digitalRead(COOK_BUTTON_PIN) == LOW && 
      millis() - lastButtonPressTime > DEBOUNCE_DELAY) {
    lastButtonPressTime = millis();
    Serial.println("Button pressed!");
    return true;
  }
  return false;
}

void handleCorrectAnswer() {
  currentScore++;
  unsigned long responseTime = millis() - promptStartTime;
  
  Serial.print("✓ CORRECT! Score: ");
  Serial.print(currentScore);
  Serial.print(" (Response time: ");
  Serial.print(responseTime);
  Serial.println("ms)");
  
  // Play success sound
  playMP3(MP3_CORRECT);
  
  // Blink LED rapidly for success feedback
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(50);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(50);
  }
  
  // Check if player won
  if (currentScore >= MAX_SCORE) {
    Serial.println("\n!!! VICTORY !!!");
    gameState = GAME_OVER;
    return;
  }
  
  // Increase difficulty
  if (currentScore % TIME_DECREASE_INTERVAL == 0 && currentTimeLimit > MIN_TIME_LIMIT) {
    currentTimeLimit -= TIME_DECREASE_AMOUNT;
    if (currentTimeLimit < MIN_TIME_LIMIT) {
      currentTimeLimit = MIN_TIME_LIMIT;
    }
    Serial.print("⚡ SPEED INCREASED! New time limit: ");
    Serial.print(currentTimeLimit);
    Serial.println("ms");
  }
  
  delay(500);  // Brief pause before next prompt
  issueNewPrompt();
}

void handleWrongAnswer() {
  Serial.println("✗ WRONG or TOO SLOW!");
  
  // Play failure sound
  playMP3(MP3_WRONG);
  delay(1000);
  
  gameState = GAME_OVER;
}

void handleGameOver() {
  Serial.println("\n==================================");
  Serial.print("GAME OVER! Final Score: ");
  Serial.println(currentScore);
  Serial.println("==================================");
  
  if (currentScore >= MAX_SCORE) {
    // Victory!
    Serial.println("🏆 YOU'RE A MASTER BAKER! 🏆");
    playMP3(MP3_HIGH_SCORE);
    
    // Victory LED pattern
    for (int i = 0; i < 5; i++) {
      digitalWrite(STATUS_LED_PIN, HIGH);
      delay(200);
      digitalWrite(STATUS_LED_PIN, LOW);
      delay(200);
    }
  } else {
    // Game over
    Serial.println("Try again to become a master baker!");
    playMP3(MP3_GAME_OVER);
    
    // Game over LED pattern
    for (int i = 0; i < 3; i++) {
      digitalWrite(STATUS_LED_PIN, HIGH);
      delay(500);
      digitalWrite(STATUS_LED_PIN, LOW);
      delay(500);
    }
  }
  
  delay(3000);
  
  // Return to idle
  gameState = IDLE;
  Serial.println("\nPress START button to play again!\n");
}

// ========== DFPlayer FUNCTIONS ==========

void playMP3(int fileNumber) {
  sendDFPlayerCommand(0x03, 0, fileNumber);
  Serial.print("♪ Playing MP3 file #");
  Serial.println(fileNumber);
}

void sendDFPlayerCommand(byte command, byte param1, byte param2) {
  // DFPlayer command packet structure
  byte buffer[10] = {0x7E, 0xFF, 0x06, command, 0x00, param1, param2, 0x00, 0x00, 0xEF};
  
  // Calculate checksum
  int checksum = -(buffer[1] + buffer[2] + buffer[3] + buffer[4] + buffer[5] + buffer[6]);
  buffer[7] = (checksum >> 8) & 0xFF;
  buffer[8] = checksum & 0xFF;
  
  // Send command
  for (int i = 0; i < 10; i++) {
    dfPlayerSerial.write(buffer[i]);
  }
  
  delay(50);  // Allow DFPlayer to process
}