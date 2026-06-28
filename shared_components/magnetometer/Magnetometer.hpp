#pragma once
#include "qmc5883p.h"

class MagnetometerClass {
private:
    MagnetometerClass() = default;

    static constexpr uint8_t MAGNETOMETER_ADDR = 0x2C;
public:
    ~MagnetometerClass() = delete;

    static MagnetometerClass* getInstancePtr();

    static MagnetometerClass& getInstance();

    void begin();

    bool isAvailable();

    bool isOverflowing();

    qmc5883p_data_t readData();

    // Returns compass heading in degrees [0, 360), 0 = magnetic north
    float getHeading();

    uint8_t getRegCTRL1();
    uint8_t getRegCTRL2();

private:
    qmc5883p_dev_t magnetometerHandle = {};

};

extern MagnetometerClass& Magnetometer;
