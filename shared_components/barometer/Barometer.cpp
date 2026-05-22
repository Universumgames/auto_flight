#include "Barometer.hpp"

#include "i2c_manager.hpp"

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
    bme280_default_init(bme280Handle);
}


float BarometerClass::getPressure() const {
    float pressure;
    bme280_read_pressure(bme280Handle, &pressure);
    return pressure;
}

float BarometerClass::getTemperature() const {
    float temperature;
    bme280_read_temperature(bme280Handle, &temperature);
    return temperature;
}

float BarometerClass::getHumidity() const {
    float humidity;
    bme280_read_humidity(bme280Handle, &humidity);
    return humidity;
}

float BarometerClass::getAltitude(float groundPressure) const {
    float pressure = getPressure();


    // Calculate altitude using the hydrostatic formula
    // h = (p0 - p) / (rho * g)
    // where:
    // p0 = standard sea level pressure (1013.25 hPa)
    // p = measured pressure
    // rho = air density at sea level (~1.225 kg/m^3)
    // g = acceleration due to gravity (~9.80665 m/s^2)

    constexpr float rho = 1.225; // kg/m^3
    constexpr float g = 9.80665; // m/s^2

    float altitude = (groundPressure - pressure) / (rho * g);
    return altitude;
}

float BarometerClass::getEstimatedAltitude() const {
    return getAltitude(p_0);
}
