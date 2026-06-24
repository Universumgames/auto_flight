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
    esp_err_t err = qmc5883p_init(&magnetometerHandle, I2CManager::getBus(), MAGNETOMETER_ADDR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_MAGNETOMETER, "init failed: %s", esp_err_to_name(err));
    }

    uint8_t chip_id = 0;
    esp_err_t ret = qmc5883p_get_chip_id(&magnetometerHandle, &chip_id);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    if(chip_id != QMC5883P_CHIP_ID_VAL) {
        ESP_LOGE(TAG_MAGNETOMETER, "Unexpected Chip ID for QMC5883P, was %02x, expected %02x", chip_id, QMC5883P_CHIP_ID_VAL);
    }

    qmc5883p_set_mode(&magnetometerHandle, QMC5883P_MODE_CONTINUOUS);
    qmc5883p_set_range(&magnetometerHandle, QMC5883P_RNG_2G);
    qmc5883p_set_odr(&magnetometerHandle, QMC5883P_ODR_200HZ);
    /// register dump
    uint8_t reg_dump[11] = {};
    ret = qmc5883p_read_register(&magnetometerHandle, QMC5883P_REG_CHIP_ID, reg_dump, 10);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    ret = qmc5883p_read_register(&magnetometerHandle, QMC5883P_REG_SIGN, reg_dump + 10, 1);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    ESP_LOGI(TAG_MAGNETOMETER, "QMC5883P register dump:");
    for(int i = 0; i < 11; i++) {
        ESP_LOGI(TAG_MAGNETOMETER, "  Reg %02x: %02x", QMC5883P_REG_CHIP_ID + i, reg_dump[i]);
    }

    ESP_LOGI(TAG_MAGNETOMETER, "Data ready? %d", isAvailable());
}

qmc5883p_data_t MagnetometerClass::readData() {
    qmc5883p_data_t data = {};
    esp_err_t err = qmc5883p_read_data(&magnetometerHandle, &data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_MAGNETOMETER, "read failed: %d", err);
    }
    return data;
}

float MagnetometerClass::getHeading() {
    auto data = readData();
    float heading = atan2f(data.y, data.x) * (180.0f / M_PI);
    if (heading < 0.0f) heading += 360.0f;
    return heading;
}

bool MagnetometerClass::isAvailable() {
    bool ready;
    esp_err_t err = qmc5883p_is_data_ready(&magnetometerHandle, &ready);
    ESP_ERROR_CHECK_WITHOUT_ABORT(err);
    return err == ESP_OK && ready;
}

bool MagnetometerClass::isOverflowing() {
    uint8_t status;
    esp_err_t err = qmc5883p_read_register(&magnetometerHandle, QMC5883P_REG_STATUS, &status, 1);
    ESP_ERROR_CHECK_WITHOUT_ABORT(err);
    return err == ESP_OK && (status & 0x02) != 0;
}

uint8_t MagnetometerClass::getRegCTRL1() {
    uint8_t ctrl1;
    esp_err_t err = qmc5883p_read_register(&magnetometerHandle, QMC5883P_REG_CTRL1, &ctrl1, 1);
    ESP_ERROR_CHECK_WITHOUT_ABORT(err);
    return (err == ESP_OK) ? ctrl1 : 0xFF;
}

uint8_t MagnetometerClass::getRegCTRL2() {
    uint8_t ctrl2;
    esp_err_t err = qmc5883p_read_register(&magnetometerHandle, QMC5883P_REG_CTRL2, &ctrl2, 1);
    ESP_ERROR_CHECK_WITHOUT_ABORT(err);
    return (err == ESP_OK) ? ctrl2 : 0xFF;
}
