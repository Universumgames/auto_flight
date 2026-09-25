#pragma once
#include "sdkconfig.h"
#include <cstdint>
#include <esp_err.h>

// The sensor driver is selected via Kconfig (Component config -> Magnetometer),
// which defines exactly one of the CONFIG_MAGNETOMETER_DRIVER_* macros.
#if defined(CONFIG_MAGNETOMETER_DRIVER_HMC5883L)
#include "hmc5883l.h"
#elif defined(CONFIG_MAGNETOMETER_DRIVER_QMC5883P_DURUOFU)
#include "qmc5883p.h"
#elif defined(CONFIG_MAGNETOMETER_DRIVER_QMC5883P_EIL)
#include "qmc5883p_eil.h"
#else
#error "No magnetometer driver selected, set CONFIG_MAGNETOMETER_DRIVER_* in menuconfig"
#endif

struct MagnetometerRawData {
    int16_t x;
    int16_t y;
    int16_t z;
};

// Magnetic field in milligauss
struct MagnetometerData {
    float x;
    float y;
    float z;
};

class MagnetometerClass {
private:
    MagnetometerClass() = default;

public:
    ~MagnetometerClass() = delete;

    static MagnetometerClass* getInstancePtr();

    static MagnetometerClass& getInstance();

    void begin();

    bool isAvailable();

    // HMC5883L has no overflow flag, there it's derived from the -4096
    // marker value the sensor writes into an overflowing axis register
    bool isOverflow();

    MagnetometerData readData();

    // Returns compass heading in degrees [0, 360), 0 = magnetic north
    float getHeading();

private:
    // Driver specific parts, implemented once per CONFIG_MAGNETOMETER_DRIVER_*
    void freeDriver();
    esp_err_t createDriver();
    esp_err_t initDriver();
    void configureDriver();
    esp_err_t readDriver(MagnetometerRawData& raw, MagnetometerData& mg);

    // Cheap magnetometer modules can silently latch up after an I2C bus glitch:
    // they keep ACK-ing reads and reporting DRDY, but the measurement
    // registers stop updating and every read returns the exact same value.
    // That isn't an I2C error, so it has to be detected by comparing
    // consecutive readings instead.
    void checkForStaleData(const MagnetometerRawData& raw, bool readOk);

#if defined(CONFIG_MAGNETOMETER_DRIVER_HMC5883L)
    hmc5883l_dev_t dev = {};
#elif defined(CONFIG_MAGNETOMETER_DRIVER_QMC5883P_DURUOFU)
    qmc5883p_dev_t dev = {};
#elif defined(CONFIG_MAGNETOMETER_DRIVER_QMC5883P_EIL)
    qmc5883p_eil_dev_t dev = {};
#endif
    MagnetometerRawData lastRaw = {};
    bool haveLastRaw = false;
    uint16_t identicalReadingCount = 0;
    static constexpr uint16_t MAX_IDENTICAL_READINGS = 40; // ~2s at 20Hz poll rate
};

extern MagnetometerClass& Magnetometer;
