#if false
#include <Arduino.h>
#include <Servo.h>

Servo servo1;

void setup() {
    servo1.attach(10);
}
int i = 1000;

void loop() {
    servo1.writeMicroseconds(i);
    i += 10;
    if (i > 2000) i = 1000;
    delay(100);
}
#endif