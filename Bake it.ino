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

#if (defined(ARDUINO_AVR_UNO) || defined(ESP8266))
#include <SoftwareSerial.h>
SoftwareSerial softSerial(9, 10); // rx, tx
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
#define ROTARY_POT_PIN  A3
#define LINEAR_POT_PIN  A2
#define BUTTON_PIN      2
#define START_BUTTON_PIN 4
#define LED1 8
#define LED2 7

#define ROTARY_THRESHOLD 300
#define LINEAR_THRESHOLD 200
#define DEBOUNCE_DELAY 20

// ================= STATE =================
bool gameRunning = false;
bool gameInitialized = false;
bool introFinished = false;

bool rotaryTriggered = false;
bool linearTriggered = false;

int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

int startButtonState = HIGH;
int lastStartButtonState = HIGH;
unsigned long lastStartDebounceTime = 0;

int score = 0;

// ================= DFPLAYER =================
DFRobotDFPlayerMini myDFPlayer;
unsigned long introStartTime = 0;
const unsigned long INTRO_DURATION = 4000; // length of 0001.mp3 in ms

unsigned long promptStartTime = 0;
unsigned long promptTimeout = 5000;

enum PromptType { NONE, SLIDE, ROTATE, PRESS };
PromptType currentPrompt = NONE;

// ================= OLED =================
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

void displayGameOver() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(F("YOU LOSE"));
  display.display();
}

// ================= GAME =================
void incrementScore() {
  score++;
  displayScore();
  if(promptTimeout > 2000) promptTimeout -= 200; // speed up game
}

PromptType getRandomPrompt() {
  int r = random(1, 4); // 1=SLIDE,2=ROTATE,3=PRESS
  return (PromptType)r;
}

void startNextPrompt() {
  currentPrompt = getRandomPrompt();
  promptStartTime = millis();
  switch(currentPrompt){
    case SLIDE: myDFPlayer.play(2); break; // cut it
    case ROTATE: myDFPlayer.play(3); break; // mix it
    case PRESS: myDFPlayer.play(4); break; // bake it
    default: break;
  }
}

// ================= INPUT HANDLERS =================
void handleStartButton() {
  int reading = digitalRead(START_BUTTON_PIN);

  if(reading != lastStartButtonState) lastStartDebounceTime = millis();

  if((millis() - lastStartDebounceTime) > DEBOUNCE_DELAY){
    if(reading != startButtonState){
      startButtonState = reading;
      if(startButtonState == LOW && introFinished){
        if(!gameInitialized){
          gameInitialized = true;
          gameRunning = true;
          score = 0;
          rotaryTriggered = false;
          linearTriggered = false;
          displayScore();
          startNextPrompt();
          Serial.println("Game started!");
        } else {
          gameRunning = !gameRunning;
          if(gameRunning){
            displayScore();
            Serial.println("Game resumed!");
          } else {
            Serial.println("Game paused!");
          }
        }
      }
    }
  }

  lastStartButtonState = reading;
}

void handleScoreButton() {
  int reading = digitalRead(BUTTON_PIN);
  if(reading != lastButtonState) lastDebounceTime = millis();

  if((millis() - lastDebounceTime) > DEBOUNCE_DELAY){
    if(reading != buttonState){
      buttonState = reading;
      if(buttonState == LOW && currentPrompt == PRESS){
        incrementScore();
        startNextPrompt();
      }
    }
  }
  lastButtonState = reading;
}

void handleRotaryPot() {
  int rotaryValue = analogRead(ROTARY_POT_PIN);
  if(rotaryValue > ROTARY_THRESHOLD && !rotaryTriggered){
    rotaryTriggered = true;
    digitalWrite(LED1, HIGH);
    delay(100);
    digitalWrite(LED1, LOW);
    if(currentPrompt == ROTATE){
      incrementScore();
      startNextPrompt();
    }
  } else if(rotaryValue <= ROTARY_THRESHOLD) rotaryTriggered = false;
}

void handleLinearPot() {
  int linearValue = analogRead(LINEAR_POT_PIN);
  if(linearValue > LINEAR_THRESHOLD && !linearTriggered){
    linearTriggered = true;
    digitalWrite(LED2, HIGH);
    delay(100);
    digitalWrite(LED2, LOW);
    if(currentPrompt == SLIDE){
      incrementScore();
      startNextPrompt();
    }
  } else if(linearValue <= LINEAR_THRESHOLD) linearTriggered = false;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(ROTARY_POT_PIN, INPUT);
  pinMode(LINEAR_POT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) for(;;);
  displayPressStart();

  FPSerial.begin(9600);
  if(!myDFPlayer.begin(FPSerial,true,true)) for(;;);
  delay(500); // **wait for DFPlayer to stabilize**
  myDFPlayer.volume(20);

  myDFPlayer.play(1); // play intro fully
  introStartTime = millis();
  introFinished = false;

  randomSeed(analogRead(A0));
}

// ================= LOOP =================
void loop() {
  // Check intro timer
  if(!introFinished && (millis() - introStartTime) >= INTRO_DURATION){
    introFinished = true;
    Serial.println("Intro finished, start button now active");
  }

  handleStartButton();

  if(!gameRunning) return;

  // check if player failed current prompt
  if(currentPrompt != NONE && (millis() - promptStartTime) > promptTimeout){
    myDFPlayer.play(5); // lose audio
    displayGameOver();
    gameRunning = false;
    currentPrompt = NONE;
  }

  handleScoreButton();
  handleRotaryPot();
  handleLinearPot();
}
