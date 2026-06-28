#include "Barometer.hpp"

#include "i2c_manager.hpp"
#include "bme280.h"

static BarometerClass* barometerInstance = nullptr;

BarometerClass& Barometer = BarometerClass::getInstance();

BarometerClass* BarometerClass::getInstancePtr() {
    if (!barometerInstance) {
        barometerInstance = new BarometerClass();
    }
    return barometerInstance;
}

BarometerClass& BarometerClass::getInstance() {
    return *getInstancePtr();
}

void BarometerClass::begin() {
    auto bus = I2CManager::getBus();

    bme280Handle = bme280_create(bus, BME280_I2C_ADDRESS_DEFAULT);
    err = bme280_default_init(bme280Handle);
    I2C_ERROR_LOG("Barometer", "setup failed", err);
}

float BarometerClass::getPressure() {
    float pressure;
    err = bme280_read_pressure(bme280Handle, &pressure);
    I2C_ERROR_LOG("Barometer", "read pressure failed", err);
    return pressure;
}

float BarometerClass::getTemperature() {
    float temperature;
    err = bme280_read_temperature(bme280Handle, &temperature);
    I2C_ERROR_LOG("Barometer", "read temperature failed", err);
    return temperature;
}

float BarometerClass::getHumidity() {
    float humidity;
    err = bme280_read_humidity(bme280Handle, &humidity);
    I2C_ERROR_LOG("Barometer", "read humidity failed", err);
    return humidity;
}

float BarometerClass::getAltitude(const float groundPressure) {
    const float pressure = getPressure();
    return calculateAltitude(groundPressure, pressure);
}

float BarometerClass::calculateAltitude(const float groundPressure, const float currentPressure) {
    // Calculate altitude using the hydrostatic formula
    // h = (p0 - p) / (rho * g)
    // where:
    // p0 = standard sea level pressure (1013.25 hPa)
    // p = measured pressure
    // rho = air density at sea level (~1.225 kg/m^3)
    // g = acceleration due to gravity (~9.80665 m/s^2)

    constexpr float rho = 1.225; // kg/m^3
    constexpr float g = 9.80665; // m/s^2

    float altitude = (groundPressure - currentPressure) / (rho * g);
    return altitude;
}

float BarometerClass::getEstimatedAltitude() {
    return getAltitude(p_0);
}

bool BarometerClass::available() const {
    return err == ESP_OK;
}
