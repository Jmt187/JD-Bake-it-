int potPin = 3;
int led1 = 8;
int led2 = 7;
int slidePin = 4;
void setup() {
  pinMode(led1, OUTPUT);
  pinMode(potPin, INPUT);
  pinMode(slidePin, INPUT);
  pinMode(led2, OUTPUT);
}

void loop() {
  int score = 0;
  int potValue = analogRead(potPin);
  int slideValue = analogRead(slidePin);

  if(potValue > 300)
 {
  score = score + 1;
  digitalWrite(led1, HIGH);
  delay(100);
  digitalWrite(led1, LOW);
 }

  if(slideValue > 200)
 {
  score = score + 1;
  digitalWrite(led2, HIGH);
  delay(100);
  digitalWrite(led2, LOW);
 }

  
}
