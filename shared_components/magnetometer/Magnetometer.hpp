#pragma once
#include "hmc5883l.h"
#include <cstdint>

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
    // Cheap HMC5883L clones can silently latch up after an I2C bus glitch:
    // they keep ACK-ing reads and reporting DRDY, but the measurement
    // registers stop updating and every read returns the exact same value.
    // That isn't an I2C error, so it has to be detected by comparing
    // consecutive readings instead.
    void checkForStaleData(const hmc5883l_raw_data_t& raw, bool readOk);

    hmc5883l_dev_t dev = {};
    hmc5883l_raw_data_t lastRaw = {};
    bool haveLastRaw = false;
    uint16_t identicalReadingCount = 0;
    static constexpr uint16_t MAX_IDENTICAL_READINGS = 40; // ~2s at 20Hz poll rate
};

extern MagnetometerClass& Magnetometer;
