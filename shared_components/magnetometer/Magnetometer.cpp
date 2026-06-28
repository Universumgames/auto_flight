#include "Magnetometer.hpp"

#include "esp_log.h"
#include "i2c_manager.hpp"
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG_MAGNETOMETER = "Magnetometer";

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
    static constexpr int MAX_ATTEMPTS = 5;
    static constexpr int RETRY_DELAY_MS = 100;

    esp_err_t err;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        magnetometerHandle = {};
        err = qmc5883p_init(&magnetometerHandle, I2CManager::getBus(), MAGNETOMETER_ADDR);
        if (err == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG_MAGNETOMETER, "init attempt %d/%d failed: %s", attempt, MAX_ATTEMPTS, esp_err_to_name(err));
        if (attempt < MAX_ATTEMPTS) vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }
    I2C_ERROR_LOG(TAG_MAGNETOMETER, "init failed", err);

    err = qmc5883p_set_mode(&magnetometerHandle, QMC5883P_MODE_CONTINUOUS);
    I2C_ERROR_LOG(TAG_MAGNETOMETER, "set mode failed", err);
    qmc5883p_set_range(&magnetometerHandle, QMC5883P_RNG_2G);
    I2C_ERROR_LOG(TAG_MAGNETOMETER, "set range failed", err);
    qmc5883p_set_odr(&magnetometerHandle, QMC5883P_ODR_200HZ);
    I2C_ERROR_LOG(TAG_MAGNETOMETER, "set odr failed", err);

    ESP_LOGI(TAG_MAGNETOMETER, "init OK, data ready? %d", isAvailable());
}

qmc5883p_data_t MagnetometerClass::readData() {
    qmc5883p_data_t data = {};
    esp_err_t err = qmc5883p_read_data(&magnetometerHandle, &data);
    I2C_ERROR_LOG(TAG_MAGNETOMETER, "read data failed", err);
    return data;
}

float MagnetometerClass::getHeading() {
    auto data = readData();
    float heading = atan2f(data.y, data.x) * (180.0f / M_PI);
    if (heading < 0.0f) heading += 360.0f;
    return heading;
}

bool MagnetometerClass::isAvailable() {
    bool ready = false;
    esp_err_t err = qmc5883p_is_data_ready(&magnetometerHandle, &ready);
    return err == ESP_OK && ready;
}

bool MagnetometerClass::isOverflowing() {
    uint8_t status;
    esp_err_t err = qmc5883p_read_register(&magnetometerHandle, QMC5883P_REG_STATUS, &status, 1);
    return err == ESP_OK && (status & 0x02) != 0;
}

uint8_t MagnetometerClass::getRegCTRL1() {
    uint8_t ctrl1;
    esp_err_t err = qmc5883p_read_register(&magnetometerHandle, QMC5883P_REG_CTRL1, &ctrl1, 1);
    return (err == ESP_OK) ? ctrl1 : 0xFF;
}

uint8_t MagnetometerClass::getRegCTRL2() {
    uint8_t ctrl2;
    esp_err_t err = qmc5883p_read_register(&magnetometerHandle, QMC5883P_REG_CTRL2, &ctrl2, 1);
    return (err == ESP_OK) ? ctrl2 : 0xFF;
}
