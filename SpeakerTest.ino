#include <NeoSWSerial.h>
#include "DFRobotDFPlayerMini.h"

int led = 7;
int startButton = 8;

// Use pins 2 and 3 to communicate with DFPlayer Mini
static const uint8_t PIN_MP3_TX = 2;
static const uint8_t PIN_MP3_RX = 3;
NeoSWSerial neoSerial(PIN_MP3_RX, PIN_MP3_TX);

// Create the Player object
DFRobotDFPlayerMini player;

void setup()
{
    pinMode(led, OUTPUT);
    pinMode(startButton, INPUT);
    
    // Init USB serial port for debugging
    Serial.begin(9600);
    
    // Init serial port for DFPlayer Mini
    neoSerial.begin(9600);
    
    if (player.begin(neoSerial))
    {
        Serial.println("OK");

        // Set volume to maximum (0 to 30).
        player.volume(30);
        // Play the first MP3 file on the SD card
        player.play(1);
    } 
    else 
    {
        Serial.println("Connecting to DFPlayer Mini failed!");
    }
}

void loop() 
{
    // Your loop code here
}