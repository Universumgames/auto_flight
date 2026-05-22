#include <Arduino.h>
#include "SBUS.h"
#include <limits.h>
#include <Servo.h>
#include <Wire.h>

#warning "Motor_controller_main"

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

static int minChannel = INT_MAX;
static int maxChannel = INT_MIN;

// Scale the S.BUS channel values into the range [0, 255] for use as LED brightness values.
int getChannel(int channel) {
    int value = sbus.getChannel(channel);

    if (value < minChannel) {
        minChannel = value;
    }
    if (value > maxChannel) {
        maxChannel = value;
    }

    float result = value;

    result -= minChannel;
    result /= (maxChannel - minChannel);
    result *= 255;

    return (int)result;
}

bool isManualOverride() {
    return getChannel(5) > 120;
}

void I2C_TxHandler() {
    Wire.write(isManualOverride());
}

void I2C_RxHandler(int numBytes) {
    if (numBytes != 4) {
        Serial.println("I2C_TxHandler: Wrong number of bytes received");
        return;
    }
    desiredValueServo1 = Wire.read();
    desiredValueServo2 = Wire.read();
    desiredValueServo3 = Wire.read();
    desiredValueServo4 = Wire.read();
}

void setup() {
    // Initialize motor controller here
    sbus.begin();

    // Initialize I2C bus
    Wire.begin(I2C_ADDRESS);

    // Initialize Servos
    servo1.attach(PIN_SERVO1);
    servo2.attach(PIN_SERVO2);
    servo3.attach(PIN_SERVO3);
    servo4.attach(PIN_SERVO4);
}

ISR(TIMER2_COMPA_vect)
{
    sbus.process();
}

/**
 *
 * @param servo Servo to write to
 * @param value value between 0 and 255
 */
void writeServo(Servo& servo, int value) {
    int angle = value * 180 / 255;
    servo.write(angle);
}

void loop() {
    bool manualOverride = isManualOverride();

    if (manualOverride) {
        writeServo(servo1, getChannel(1));
        writeServo(servo2, getChannel(2));
        writeServo(servo3, getChannel(3));
        writeServo(servo4, getChannel(4));

    }else {
        writeServo(servo1, desiredValueServo1);
        writeServo(servo2, desiredValueServo2);
        writeServo(servo3, desiredValueServo3);
        writeServo(servo4, desiredValueServo4);
    }
}