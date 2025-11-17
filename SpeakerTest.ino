#include "Arduino.h"
#include <NeoSWSerial.h>
#include "DFRobotDFPlayerMini.h"

// Use pins 2 and 3 to communicate with DFPlayer Mini
static const uint8_t PIN_MP3_TX = 7; // Connects to module's RX
static const uint8_t PIN_MP3_RX = 6; // Connects to module's TX
NeoSWSerial neoSerial(PIN_MP3_RX, PIN_MP3_TX);

int button = 9;
int led = 8;

// Create the Player object
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  pinMode(button, INPUT);
  pinMode(led, OUTPUT);
  
  Serial.begin(9600);
  neoSerial.begin(9600);
  
  delay(1000); // Give DFPlayer time to initialize
  
  if (myDFPlayer.begin(neoSerial)) {
    Serial.println("DFPlayer Mini online.");
    myDFPlayer.setTimeOut(500);
    myDFPlayer.volume(20);  // Set volume value. From 0 to 30
    myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  } else {
    Serial.println("Unable to begin DFPlayer");
  }
}

void loop()
{
  int state = digitalRead(button);
  
  if (state == HIGH) {
    delay(50); // Debounce delay
    digitalWrite(led, HIGH);
    myDFPlayer.play(1); // Play track one on dfplayer
    delay(200); // Prevent multiple triggers
  } else {
    digitalWrite(led, LOW);
  }
}
