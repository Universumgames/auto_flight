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
    if (dev.dev_handle) {
        hmc5883l_free_desc(&dev);
    }
    dev = {};
    err = hmc5883l_init_desc(&dev, I2CManager::getBus());
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MAGNETOMETER, "init_desc attempt failed: %s", esp_err_to_name(err));
        return;
    }
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        err = hmc5883l_init(&dev);
        if (err == ESP_OK) break;
        ESP_LOGW(TAG_MAGNETOMETER, "init attempt %d/%d failed: %s", attempt, MAX_ATTEMPTS, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG_MAGNETOMETER, "init failed after retries: %s", esp_err_to_name(err));
        return;
    }

    // Single-measurement mode: the sensor idles between  reads instead of
    // sampling continuously, which cuts self-heating and exposure to
    // motor/ESC current noise since we only need occasional readings.
    // Each readData() call re-arms it (see below).
    err = hmc5883l_set_opmode(&dev, HMC5883L_MODE_SINGLE);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set opmode failed: %s", esp_err_to_name(err));

    err = hmc5883l_set_samples_averaged(&dev, HMC5883L_SAMPLES_8);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set samples failed: %s", esp_err_to_name(err));

    err = hmc5883l_set_gain(&dev, HMC5883L_GAIN_1090);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set gain failed: %s", esp_err_to_name(err));

    ESP_LOGI(TAG_MAGNETOMETER, "init OK, data ready? %d", isAvailable());
}

hmc5883l_data_t MagnetometerClass::readData() {
    hmc5883l_raw_data_t raw = {};
    esp_err_t err = hmc5883l_get_raw_data(&dev, &raw);
    if (err != ESP_OK)
        ESP_LOGE(TAG_MAGNETOMETER, "read data failed: %s", esp_err_to_name(err));

    checkForStaleData(raw, err == ESP_OK);

    hmc5883l_data_t data = {};
    hmc5883l_raw_to_mg(&dev, &raw, &data);
    return data;
}

void MagnetometerClass::checkForStaleData(const hmc5883l_raw_data_t& raw, bool readOk) {
    if (!readOk) {
        haveLastRaw = false;
        identicalReadingCount = 0;
        return;
    }

    if (haveLastRaw && raw.x == lastRaw.x && raw.y == lastRaw.y && raw.z == lastRaw.z) {
        identicalReadingCount++;
    } else {
        identicalReadingCount = 0;
    }

    lastRaw = raw;
    haveLastRaw = true;

    if (identicalReadingCount >= MAX_IDENTICAL_READINGS) {
        ESP_LOGW(TAG_MAGNETOMETER, "%u identical readings in a row, sensor likely latched up after a bus "
                 "glitch - forcing a bus recovery and re-init", identicalReadingCount);
        identicalReadingCount = 0;
        haveLastRaw = false;
        I2CManager::recoverBus();
        begin();
    }
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
