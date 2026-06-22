#pragma once
#include "qmc5883p.h"

class MagnetometerClass {
private:
    MagnetometerClass() = default;

    static constexpr int QMC5883P_I2C_ADDR = 0x2C;
public:
    ~MagnetometerClass() = delete;

    static MagnetometerClass* getInstancePtr();

    static MagnetometerClass& getInstance();

    void begin();

private:
    qmc5883p_dev_t magnetometerHandle;

    qmc5883p_data_t readData();

};

extern MagnetometerClass& Magnetometer;
