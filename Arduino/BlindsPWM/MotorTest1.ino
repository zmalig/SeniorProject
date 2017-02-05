
int motorDrive = 9;
int timeDelay = 5000;

void setup(){
  pinMode(motorDrive, OUTPUT);
}

void loop(){
  analogWrite(motorDrive, 255);
  delay(timeDelay);
  analogWrite(motorDrive, 128);
  delay(timeDelay);
  analogWrite(motorDrive, 0);
  delay(timeDelay);
}
