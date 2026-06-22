#include <Arduino.h>
#include "SBUS.h"
#include <Servo.h>
#include <Wire.h>

#define PIN_SERVO1 9
#define PIN_SERVO2 10
#define PIN_SERVO3 11
#define PIN_SERVO4 12

#define I2C_ADDRESS 0x42

SBUS sbus(Serial);
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

int desiredValueServo1 = 0;
int desiredValueServo2 = 0;
int desiredValueServo3 = 0;
int desiredValueServo4 = 0;

// Returns normalized channel value in [-100, 100]
int getChannel(int channel) {
    return (int)sbus.getNormalizedChannel(channel);
}

// Maps [-100, 100] to [0, 180]
int mapToServo(int value) {
    return (value + 100) * 180 / 200;
}

bool isManualOverride() {
    return getChannel(5) > 50;
}

void I2C_TxHandler() {
    Wire.write(isManualOverride());
}

void I2C_RxHandler(int numBytes) {
    if (numBytes != 4) {
        return;
    }
    desiredValueServo1 = (int8_t)Wire.read();
    desiredValueServo2 = (int8_t)Wire.read();
    desiredValueServo3 = (int8_t)Wire.read();
    desiredValueServo4 = (int8_t)Wire.read();
}

void setup() {
    sbus.begin();

    Wire.begin(I2C_ADDRESS);
    Wire.onRequest(I2C_TxHandler);
    Wire.onReceive(I2C_RxHandler);

    servo1.attach(PIN_SERVO1);
    servo2.attach(PIN_SERVO2);
    servo3.attach(PIN_SERVO3);
    servo4.attach(PIN_SERVO4);
}

ISR(TIMER2_COMPA_vect)
{
    sbus.process();
}

// value in [-100, 100]
void writeServo(Servo& servo, int value) {
    servo.write(mapToServo(value));
}

void loop() {
    bool manualOverride = isManualOverride();

    if (manualOverride) {
        writeServo(servo1, getChannel(1));
        writeServo(servo2, getChannel(2));
        writeServo(servo3, getChannel(3));
        writeServo(servo4, getChannel(4));
    } else {
        writeServo(servo1, desiredValueServo1);
        writeServo(servo2, desiredValueServo2);
        writeServo(servo3, desiredValueServo3);
        writeServo(servo4, desiredValueServo4);
    }
}
