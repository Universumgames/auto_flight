#include "Magnetometer.hpp"

#include "esp_log.h"
#include "i2c_manager.hpp"

static MagnetometerClass* magnetometerInstance = nullptr;

MagnetometerClass& Magnetometer = MagnetometerClass::getInstance();

MagnetometerClass* MagnetometerClass::getInstancePtr() {
    if (!magnetometerInstance) {
        magnetometerInstance = new MagnetometerClass();
    }
    return magnetometerInstance;
}

MagnetometerClass& MagnetometerClass::getInstance() {
    return *getInstancePtr();
}

void MagnetometerClass::begin() {
    esp_err_t err = qmc5883p_init(&magnetometerHandle, I2CManager::getBus(), QMC5883P_I2C_ADDR);
    if (err != ESP_OK) {
        ESP_LOGE("Magnetometer", "init failed: %d", err);
    }
    err = qmc5883p_soft_reset(&magnetometerHandle);
    ESP_ERROR_CHECK(err);
    err = qmc5883p_set_mode(&magnetometerHandle, QMC5883P_MODE_CONTINUOUS);
    ESP_ERROR_CHECK(err);
    err = qmc5883p_set_range(&magnetometerHandle, QMC5883P_RNG_2G);
    ESP_ERROR_CHECK(err);
    err = qmc5883p_set_odr(&magnetometerHandle, QMC5883P_ODR_10HZ);
    ESP_ERROR_CHECK(err);
}

qmc5883p_data_t MagnetometerClass::readData() {
    qmc5883p_data_t data = {};
    esp_err_t err = qmc5883p_read_data(&magnetometerHandle, &data);
    if (err != ESP_OK) {
        ESP_LOGE("Magnetometer", "read failed: %d", err);
    }
    return data;
}
