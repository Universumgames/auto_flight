#pragma once
#include "esp_err.h"


class BarometerClass {
private:
    BarometerClass() = default;

public:
    ~BarometerClass() = delete;

    static BarometerClass& getInstance();
    static BarometerClass* getInstancePtr();

    void begin();

    bool initialized() const;

    /// Get current temperature in C
    float getTemperature() const;
    /// Get current pressure in hPa
    float getPressure() const;
    /// Get current relative humidity
    float getHumidity() const;

    float getEstimatedAltitude() const;
    float getAltitude(float groundPressure) const;

    static float calculateAltitude(float groundPressure, float currentPressure);

private:
    void* bme280Handle = nullptr;
    esp_err_t err = ESP_FAIL;

    static constexpr float p_0 = 1013.25; //hPa, assumed standard sea level pressure
};

extern BarometerClass& Barometer;
