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

    esp_err_t err = ESP_FAIL;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        dev = {};
        err = hmc5883l_init_desc(&dev, I2CManager::getBus());
        if (err != ESP_OK) {
            ESP_LOGW(TAG_MAGNETOMETER, "init_desc attempt %d/%d failed: %s", attempt, MAX_ATTEMPTS, esp_err_to_name(err));
            if (attempt < MAX_ATTEMPTS) vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }
        err = hmc5883l_init(&dev);
        if (err == ESP_OK) break;
        ESP_LOGW(TAG_MAGNETOMETER, "init attempt %d/%d failed: %s", attempt, MAX_ATTEMPTS, esp_err_to_name(err));
        hmc5883l_free_desc(&dev);
        if (attempt < MAX_ATTEMPTS) vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG_MAGNETOMETER, "init failed after retries: %s", esp_err_to_name(err));
        return;
    }

    err = hmc5883l_set_opmode(&dev, HMC5883L_MODE_CONTINUOUS);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set opmode failed: %s", esp_err_to_name(err));

    err = hmc5883l_set_samples_averaged(&dev, HMC5883L_SAMPLES_8);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set samples failed: %s", esp_err_to_name(err));

    err = hmc5883l_set_data_rate(&dev, HMC5883L_DATA_RATE_75_00);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set data rate failed: %s", esp_err_to_name(err));

    err = hmc5883l_set_gain(&dev, HMC5883L_GAIN_1090);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set gain failed: %s", esp_err_to_name(err));

    ESP_LOGI(TAG_MAGNETOMETER, "init OK, data ready? %d", isAvailable());
}

hmc5883l_data_t MagnetometerClass::readData() {
    hmc5883l_data_t data = {};
    esp_err_t err = hmc5883l_get_data(&dev, &data);
    if (err != ESP_OK)
        ESP_LOGE(TAG_MAGNETOMETER, "read data failed: %s", esp_err_to_name(err));
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
    esp_err_t err = hmc5883l_data_is_ready(&dev, &ready);
    return err == ESP_OK && ready;
}

bool MagnetometerClass::isLocked() {
    bool locked = false;
    esp_err_t err = hmc5883l_data_is_locked(&dev, &locked);
    return err == ESP_OK && locked;
}
