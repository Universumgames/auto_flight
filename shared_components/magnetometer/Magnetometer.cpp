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

    freeDriver();
    esp_err_t err = createDriver();
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MAGNETOMETER, "init_desc attempt failed: %s", esp_err_to_name(err));
        return;
    }
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        err = initDriver();
        if (err == ESP_OK) break;
        ESP_LOGW(TAG_MAGNETOMETER, "init attempt %d/%d failed: %s", attempt, MAX_ATTEMPTS, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG_MAGNETOMETER, "init failed after retries: %s", esp_err_to_name(err));
        return;
    }

    configureDriver();

    ESP_LOGI(TAG_MAGNETOMETER, "init OK, data ready? %d", isAvailable());
}

MagnetometerData MagnetometerClass::readData() {
    MagnetometerRawData raw = {};
    MagnetometerData data = {};
    esp_err_t err = readDriver(raw, data);
    if (err != ESP_OK)
        ESP_LOGE(TAG_MAGNETOMETER, "read data failed: %s", esp_err_to_name(err));

    checkForStaleData(raw, err == ESP_OK);

    // Board is mounted chip-side down with X still aligned to the nose (180 deg
    // rotation about X relative to the sensor's native frame), same as the
    // MPU6050 on this combo board - flip Y and Z to match, see Gyroscope.cpp.
    data.y *= -1;
    data.z *= -1;

    return data;
}

void MagnetometerClass::checkForStaleData(const MagnetometerRawData& raw, bool readOk) {
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

#if defined(CONFIG_MAGNETOMETER_DRIVER_HMC5883L)

void MagnetometerClass::freeDriver() {
    if (dev.dev_handle) hmc5883l_free_desc(&dev);
    dev = {};
}

esp_err_t MagnetometerClass::createDriver() {
    return hmc5883l_init_desc(&dev, I2CManager::getBus());
}

esp_err_t MagnetometerClass::initDriver() {
    return hmc5883l_init(&dev);
}

void MagnetometerClass::configureDriver() {
    // Single-measurement mode: the sensor idles between reads instead of
    // sampling continuously, which cuts self-heating and exposure to
    // motor/ESC current noise since we only need occasional readings.
    // Each readData() call re-arms it.
    esp_err_t err = hmc5883l_set_opmode(&dev, HMC5883L_MODE_SINGLE);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set opmode failed: %s", esp_err_to_name(err));

    err = hmc5883l_set_samples_averaged(&dev, HMC5883L_SAMPLES_8);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set samples failed: %s", esp_err_to_name(err));

    err = hmc5883l_set_gain(&dev, HMC5883L_GAIN_1090);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set gain failed: %s", esp_err_to_name(err));
}

esp_err_t MagnetometerClass::readDriver(MagnetometerRawData& raw, MagnetometerData& mg) {
    hmc5883l_raw_data_t r = {};
    esp_err_t err = hmc5883l_get_raw_data(&dev, &r);
    hmc5883l_data_t d = {};
    hmc5883l_raw_to_mg(&dev, &r, &d);
    raw = {r.x, r.y, r.z};
    mg = {d.x, d.y, d.z};
    return err;
}

bool MagnetometerClass::isAvailable() {
    bool ready = false;
    esp_err_t err = hmc5883l_data_is_ready(&dev, &ready);
    ESP_LOGD(TAG_MAGNETOMETER, "isAvailable: %d", err);
    return err == ESP_OK && ready;
}

bool MagnetometerClass::isOverflow() {
    static constexpr int16_t HMC5883L_OVERFLOW = -4096;
    return haveLastRaw &&
           (lastRaw.x == HMC5883L_OVERFLOW || lastRaw.y == HMC5883L_OVERFLOW || lastRaw.z == HMC5883L_OVERFLOW);
}

#elif defined(CONFIG_MAGNETOMETER_DRIVER_QMC5883P_DURUOFU)

void MagnetometerClass::freeDriver() {
    if (dev.dev_handle) i2c_bus_device_delete(&dev.dev_handle);
    dev = {};
}

esp_err_t MagnetometerClass::createDriver() {
    // qmc5883p_init() creates the I2C device itself
    return ESP_OK;
}

esp_err_t MagnetometerClass::initDriver() {
    // Drop the device a failed previous attempt left behind
    freeDriver();
    // Configures continuous mode, 200 Hz, OSR2=8, +-8 G
    return qmc5883p_init(&dev, I2CManager::getBus(), QMC5883P_I2C_ADDR_DEF);
}

void MagnetometerClass::configureDriver() {
    // +-2 G range, the closest match to the HMC5883L +-1.3 G setting
    esp_err_t err = qmc5883p_set_range(&dev, QMC5883P_RNG_2G);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set range failed: %s", esp_err_to_name(err));
}

esp_err_t MagnetometerClass::readDriver(MagnetometerRawData& raw, MagnetometerData& mg) {
    qmc5883p_data_t d = {};
    esp_err_t err = qmc5883p_read_data(&dev, &d);
    raw = {d.raw_x, d.raw_y, d.raw_z};
    // Driver reports gauss
    mg = {d.x * 1000.0f, d.y * 1000.0f, d.z * 1000.0f};
    return err;
}

bool MagnetometerClass::isAvailable() {
    bool ready = false;
    esp_err_t err = qmc5883p_is_data_ready(&dev, &ready);
    ESP_LOGD(TAG_MAGNETOMETER, "isAvailable: %d", err);
    return err == ESP_OK && ready;
}

bool MagnetometerClass::isOverflow() {
    uint8_t status = 0;
    esp_err_t err = qmc5883p_read_register(&dev, QMC5883P_REG_STATUS, &status, 1);
    return err == ESP_OK && (status & 0x02);
}

#elif defined(CONFIG_MAGNETOMETER_DRIVER_QMC5883P_EIL)

void MagnetometerClass::freeDriver() {
    if (dev.dev_handle) qmc5883p_eil_free_desc(&dev);
    dev = {};
}

esp_err_t MagnetometerClass::createDriver() {
    return qmc5883p_eil_init_desc(&dev, I2CManager::getBus());
}

esp_err_t MagnetometerClass::initDriver() {
    return qmc5883p_eil_init(&dev);
}

void MagnetometerClass::configureDriver() {
    // Single-measurement mode, see the HMC5883L variant for the reasoning
    esp_err_t err = qmc5883p_eil_set_opmode(&dev, QMC5883P_EIL_MODE_SINGLE);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set opmode failed: %s", esp_err_to_name(err));

    err = qmc5883p_eil_set_samples_averaged(&dev, QMC5883P_EIL_SAMPLES_8);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set samples failed: %s", esp_err_to_name(err));

    // +-2 G range, the closest match to the HMC5883L +-1.3 G setting
    err = qmc5883p_eil_set_gain(&dev, QMC5883P_EIL_GAIN_15000);
    if (err != ESP_OK) ESP_LOGE(TAG_MAGNETOMETER, "set gain failed: %s", esp_err_to_name(err));
}

esp_err_t MagnetometerClass::readDriver(MagnetometerRawData& raw, MagnetometerData& mg) {
    qmc5883p_eil_raw_data_t r = {};
    esp_err_t err = qmc5883p_eil_get_raw_data(&dev, &r);
    qmc5883p_eil_data_t d = {};
    qmc5883p_eil_raw_to_mg(&dev, &r, &d);
    raw = {r.x, r.y, r.z};
    mg = {d.x, d.y, d.z};
    return err;
}

bool MagnetometerClass::isAvailable() {
    bool ready = false;
    esp_err_t err = qmc5883p_eil_data_is_ready(&dev, &ready);
    ESP_LOGD(TAG_MAGNETOMETER, "isAvailable: %d", err);
    return err == ESP_OK && ready;
}

bool MagnetometerClass::isOverflow() {
    bool overflow = false;
    esp_err_t err = qmc5883p_eil_data_is_overflow(&dev, &overflow);
    return err == ESP_OK && overflow;
}

#endif
