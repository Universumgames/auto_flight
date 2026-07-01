#pragma once
#include "hmc5883l.h"

class MagnetometerClass {
private:
    MagnetometerClass() = default;

public:
    ~MagnetometerClass() = delete;

    static MagnetometerClass* getInstancePtr();

    static MagnetometerClass& getInstance();

    void begin();

    bool isAvailable();

    bool isLocked();

    hmc5883l_data_t readData();

    // Returns compass heading in degrees [0, 360), 0 = magnetic north
    float getHeading();

private:
    hmc5883l_dev_t dev = {};
};

extern MagnetometerClass& Magnetometer;
