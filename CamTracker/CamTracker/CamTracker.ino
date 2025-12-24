#include <Servo.h>

// Create servo objects
Servo baseServo;
Servo shoulderServo;
Servo elbowServo;
Servo gripperServo;

void setup() {
  Serial.begin(9600);
  
  // Attach servos to digital pins
  baseServo.attach(9);
  shoulderServo.attach(10);
  elbowServo.attach(11);
  gripperServo.attach(6);

  // Initialize to home position
  baseServo.write(90);
  shoulderServo.write(90);
  elbowServo.write(90);
  gripperServo.write(0);
}

void loop() {
  if (Serial.available() > 0) {
    // Read the incoming character (the identifier)
    char c = Serial.read();

    // Check which servo the value belongs to
    if (c == 'B') {
      int val = Serial.parseInt();
      baseServo.write(val);
    } 
    else if (c == 'S') {
      int val = Serial.parseInt();
      shoulderServo.write(val);
    } 
    else if (c == 'E') {
      int val = Serial.parseInt();
      elbowServo.write(val);
    } 
    else if (c == 'G') {
      int val = Serial.parseInt();
      gripperServo.write(val);
    }
  }
}