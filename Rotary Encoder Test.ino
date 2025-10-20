const int potPin = 26;
int ledPin = 14;
int ledValue = 0;

void setup()
{
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
)

void loop()
{
  //int ledValue = 0;
  int potValue = analogRead(potPin);
  Serial.print("Potentiometer Value: "); // to be removed
  Serial.pintln(potValue); //to be removed

//if(potVal > 30)
//{
//  ledValue = 1;
//}

  delay(100);
}
