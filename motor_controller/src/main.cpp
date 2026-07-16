#include <Arduino.h>
#include "SBUS.h"
#include <Servo.h>
#include <Wire.h>

#define PIN_SERVO1 9
#define PIN_SERVO2 10
#define PIN_SERVO3 11
#define PIN_SERVO4 12

#define I2C_ADDRESS 0x42

#define I2C_RESET_TIMEOUT_MILLIS (1000L)

#define DEBUG_I2C
#define DEBUG_PLANE
#define VIRTUAL_DEBUG
//#define DEBUG_SERVO

SBUS sbus(Serial);
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

int lastServoValues[4] = {0, 0, 0, 0};

int desiredValueServo1 = 0;
int desiredValueServo2 = 0;
int desiredValueServo3 = 0;
int desiredValueServo4 = 0;

unsigned long lastI2CMessageTime = 0;

// Returns normalized channel value in [-100, 100]
int getChannel(int channel) {
    return (int)sbus.getNormalizedChannel(channel);
}

int clamp(int value, int min, int max) {
    if (value < min) value = min;
    if (value > max) value = max;
    return value;
}

// Maps [-100, 100] to RC PWM range [1000, 2000] µs (center 0 → 1500 µs)
int mapToServo(int value) {
    int clamped = clamp(value, -100, 100);
    return 1500 + clamped * 5;
}

bool isManualOverride() {
#if defined(DEBUG_PLANE) || defined(DEBUG_I2C) || defined(VIRTUAL_DEBUG)
    return false;
#else
    return getChannel(5) > 50;
#endif
}

void I2C_TxHandler() {
    Wire.write(isManualOverride());
}

void I2C_RxHandler(int numBytes) {
#if defined(DEBUG_I2C) && !defined(VIRTUAL_DEBUG)
    Serial.print("I2C_RxHandler called with ");
    Serial.print(numBytes);
    Serial.println(" bytes");
#endif

    char buf[4];
    int bytesRead = Wire.readBytes(buf, 4);
    if (bytesRead != 4) {
        // Handle error: not enough bytes received
        return;
    }
    desiredValueServo1 = (int8_t)buf[0];
    desiredValueServo2 = (int8_t)buf[1];
    desiredValueServo3 = (int8_t)buf[2];
    desiredValueServo4 = (int8_t)buf[3];
#if defined(DEBUG_I2C) || defined(VIRTUAL_DEBUG)
    Serial.print(desiredValueServo1);
    Serial.print(",");
    Serial.print(desiredValueServo2);
    Serial.print(",");
    Serial.print(desiredValueServo3);
    Serial.print(",");
    Serial.println(desiredValueServo4);
#endif
    lastI2CMessageTime = millis();
}

// Hardware I2C pins on the Nano (ATmega328) - not remappable.
#define PIN_I2C_SDA A4
#define PIN_I2C_SCL A5

void setupI2C() {
    // clear internal I2C state machine
    Wire.end();

    // Wire.end() disables the TWI peripheral but doesn't guarantee SDA/SCL
    // come back up cleanly - if the AVR was clock-stretching (holding SCL
    // low) at the moment TWEN got cleared, the pin can stay low afterward.
    // Explicitly release both lines to inputs with pull-ups so we never hand
    // a stuck-low line back to a bus shared with other devices (magnetometer
    // etc.) while TWI is offline.
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    delay(10);

    Wire.begin(I2C_ADDRESS);
    Wire.onRequest(I2C_TxHandler);
    Wire.onReceive(I2C_RxHandler);

    lastI2CMessageTime = millis();
}

// value in [-100, 100]
void writeServo(Servo& servo, int value, int id) {
    int pulseWidth = mapToServo(value);
    if (lastServoValues[id - 1] != pulseWidth) {
        servo.writeMicroseconds(pulseWidth);
        lastServoValues[id - 1] = pulseWidth;
    }
}

void setup() {
#if defined(VIRTUAL_DEBUG) || defined(DEBUG_I2C)
    Serial.begin(115200);
#else
    sbus.begin();
#endif

    setupI2C();

    servo1.attach(PIN_SERVO1);
    servo2.attach(PIN_SERVO2);
    servo3.attach(PIN_SERVO3);
    servo4.attach(PIN_SERVO4);

#ifdef DEBUG_SERVO
    int i = -100;
    while (true) {
        Serial.println("DEBUG_SERVO: Servo pins initialized.");
        i++;
        if (i > 100) i = -100;
        writeServo(servo1, i, 1);
        writeServo(servo2, i, 2);
        writeServo(servo3, i, 3);
        writeServo(servo4, i, 4);
        delay(50);
    }
#endif
}

#if !defined(VIRTUAL_DEBUG) && !defined(DEBUG_I2C)
ISR(TIMER2_COMPA_vect)
{
    sbus.process();
}
#endif

void loop() {
    bool manualOverride = isManualOverride();

    int channel1 = manualOverride? getChannel(1) : desiredValueServo1;
    int channel2 = manualOverride? getChannel(2) : desiredValueServo2;
    int channel3 = manualOverride? getChannel(3) : desiredValueServo3;
    int channel4 = manualOverride? getChannel(4) : desiredValueServo4;
    writeServo(servo1, channel1, 1);
    writeServo(servo2, channel2, 2);
    writeServo(servo3, channel3, 3);
    writeServo(servo4, channel4, 4);

    if (lastI2CMessageTime < (millis() - I2C_RESET_TIMEOUT_MILLIS)) {
        setupI2C();
    }
}
