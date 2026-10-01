#if true

#include <Arduino.h>
#include "SBUS.h"
#include <Servo.h>
#include <Wire.h>

#define PIN_SERVO1 9
#define PIN_SERVO2 10
#define PIN_SERVO3 11
#define PIN_SERVO4 12

#define I2C_ADDRESS 0x42

#define I2C_RESET_TIMEOUT_MILLIS (5000L)

#define DEBUG_I2C // serial logging if I2C messages arrive
#define DEBUG_PLANE // no need for SBUS channel 5, manual override is always disabled
#define VIRTUAL_DEBUG // output only the servo control signals to serial [-100,100]
//#define DEBUG_SERVO // sweeping from -100 to 100 over all servos

SBUS sbus(Serial);
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

int lastServoValues[4] = {0, 0, 0, 0};

// written from the TWI ISR
volatile int8_t desiredValueServo1 = 0;
volatile int8_t desiredValueServo2 = 0;
volatile int8_t desiredValueServo3 = 0;
volatile int8_t desiredValueServo4 = 0;

volatile unsigned long lastI2CMessageTime = 0;
volatile bool i2cFrameReceived = false;
volatile bool i2cFrameDropped = false;
volatile int lastI2CByteCount = 0;

// SDA held low longer than this (a 100 kHz byte takes ~0.1 ms) means a
// transaction got cut off and the TWI slave is waiting for clocks that
// will never come -> reinit TWI to release the line.
#define I2C_SDA_STUCK_MILLIS 25

unsigned long sdaLowSince = 0;

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

// Runs inside the TWI ISR: interrupts are disabled, so millis() is frozen.
// Never block here (no Wire.readBytes - its timeout relies on millis() and
// would spin forever on a short frame, e.g. when the master resets mid-write
// or probes with a 0-byte write) and don't print to Serial.
void I2C_RxHandler(int numBytes) {
    lastI2CByteCount = numBytes;
    if (numBytes != 4) {
        // incomplete/foreign frame: drain and drop it
        while (Wire.available()) Wire.read();
        i2cFrameDropped = true;
        return;
    }
    desiredValueServo1 = (int8_t)Wire.read();
    desiredValueServo2 = (int8_t)Wire.read();
    desiredValueServo3 = (int8_t)Wire.read();
    desiredValueServo4 = (int8_t)Wire.read();
    lastI2CMessageTime = millis();
    i2cFrameReceived = true;
}

// Hardware I2C pins on the Nano (ATmega328) - not remappable.
#define PIN_I2C_SDA A4
#define PIN_I2C_SCL A5

void setupI2C() {
    // Disabling TWEN resets the TWI state machine and hands SDA/SCL back to
    // the port registers (inputs), releasing anything the slave was driving.
    Wire.end();
    TWCR = 0;
    pinMode(PIN_I2C_SDA, INPUT);
    pinMode(PIN_I2C_SCL, INPUT);

    Wire.begin(I2C_ADDRESS);
    Wire.onRequest(I2C_TxHandler);
    Wire.onReceive(I2C_RxHandler);

    noInterrupts();
    lastI2CMessageTime = millis();
    interrupts();
    sdaLowSince = 0;
}

// Watches for a stuck bus / dead master and reinitializes TWI if needed.
void checkI2C() {
    unsigned long now = millis();

    // PINC reflects the real pin level even while TWI owns the pin
    if (digitalRead(PIN_I2C_SDA) == LOW) {
        if (sdaLowSince == 0) sdaLowSince = now ? now : 1;
        else if (now - sdaLowSince > I2C_SDA_STUCK_MILLIS) {
#if defined(DEBUG_I2C) && !defined(VIRTUAL_DEBUG)
            Serial.println("I2C: SDA stuck low, resetting TWI");
#endif
            setupI2C();
            return;
        }
    } else {
        sdaLowSince = 0;
    }

    noInterrupts();
    unsigned long last = lastI2CMessageTime;
    interrupts();
    if (now - last > I2C_RESET_TIMEOUT_MILLIS) {
        setupI2C();
    }
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

    setupI2C();
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

#if defined(DEBUG_I2C) || defined(VIRTUAL_DEBUG)
    if (i2cFrameReceived) {
        i2cFrameReceived = false;
        Serial.print(channel1);
        Serial.print(",");
        Serial.print(channel2);
        Serial.print(",");
        Serial.print(channel3);
        Serial.print(",");
        Serial.println(channel4);
    }
#endif
#if defined(DEBUG_I2C) && !defined(VIRTUAL_DEBUG)
    if (i2cFrameDropped) {
        i2cFrameDropped = false;
        Serial.print("I2C: dropped frame with ");
        Serial.print(lastI2CByteCount);
        Serial.println(" bytes");
    }
#endif

    checkI2C();
}
#endif