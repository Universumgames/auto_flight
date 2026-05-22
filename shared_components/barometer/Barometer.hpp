#pragma once
#include "bme280.h"

class BarometerClass {
private:
    BarometerClass() = default;

public:
    ~BarometerClass() = delete;

    static BarometerClass& getInstance();
    static BarometerClass* getInstancePtr();

    void begin();

    /// Get current temperature in C
    float getTemperature() const;
    /// Get current pressure in hPa
    float getPressure() const;
    /// Get current relative humidity
    float getHumidity() const;

    float getEstimatedAltitude() const;
    float getAltitude(float groundPressure) const;

private:
    bme280_handle_t bme280Handle = nullptr;

    static constexpr float p_0 = 1013.25; //hPa, assumed standard sea level pressure
};

extern BarometerClass& Barometer;
