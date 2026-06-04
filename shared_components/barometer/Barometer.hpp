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

    bool available() const;

    /// Get current temperature in C
    float getTemperature();
    /// Get current pressure in hPa
    float getPressure();
    /// Get current relative humidity
    float getHumidity();

    float getEstimatedAltitude();
    float getAltitude(float groundPressure);

    static float calculateAltitude(float groundPressure, float currentPressure);

private:
    void* bme280Handle = nullptr;
    esp_err_t err = ESP_FAIL;

    static constexpr float p_0 = 1013.25; //hPa, assumed standard sea level pressure
};

extern BarometerClass& Barometer;
