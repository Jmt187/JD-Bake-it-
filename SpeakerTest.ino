#include "Arduino.h"
#include "SoftwareSerial.h"
#include "NeoSWSerial.h"
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
  Serial.begin(9600);
  neoSerial.begin(9600);
  myDFPlayer.begin(neoSerial);
  myDFPlayer.setTimeOut(5000);

  pinMode(button, INPUT);
  pinMode(led, OUTPUT);

  delay(20);
  myDFPlayer.volume(20);  //Set volume value. From 0 to 30
  delay(20);
}

void loop()
{
   int state = digitalRead(button); //btn being read on pin A0
  if (state == HIGH) { // has pullup so LOW read is pushed
    delay(20);
    digitalWrite(led, HIGH);
    myDFPlayer.play(1); // play track one on dfplayer
  }
}
