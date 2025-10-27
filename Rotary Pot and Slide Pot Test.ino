int potPin = 3;
int led = 8;
int slidePin = 4;

void setup()
{
    pinMode(led, OUTPUT);
    pinMode(potPin, INPUT);
    pinMode(slidePin, INPUT);
}

void loop()
{
    int score = 0;
    int potValue = analogRead(potPin);
    int slideValue = analogRead(slidePin);

    if(potValue > 300)
    {
        score = score + 1;
        digitalWrite(led, HIGH);
        delay(100);
        digitalWrite(led, LOW);
    }
}