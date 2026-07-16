#include "Gyroscope.hpp"

#include <cmath>

#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_manager.hpp"
#include "freertos/FreeRTOS.h"

static GyroscopeClass* gyroscopeInstance = nullptr;

GyroscopeClass& Gyroscope = GyroscopeClass::getInstance();

#define SOFT_ERROR_CHECK(err) \
    if (err != ESP_OK) { \
        ESP_ERROR_CHECK_WITHOUT_ABORT(err); \
        return; \
    }

GyroscopeClass* GyroscopeClass::getInstancePtr() {
    if (!gyroscopeInstance) {
        gyroscopeInstance = new GyroscopeClass();
    }
    return gyroscopeInstance;
}

GyroscopeClass& GyroscopeClass::getInstance() {
    return *getInstancePtr();
}

void GyroscopeClass::begin() {
    gyroscopeHandle = mpu6050_create(I2CManager::getBus(), MPU6050_I2C_ADDRESS);
    err = mpu6050_config(gyroscopeHandle, ACCE_FS_2G, GYRO_FS_250DPS);
    I2C_ERROR_LOG("Gyroscope", "setup failed", err);
    err = mpu6050_wake_up(gyroscopeHandle);
    I2C_ERROR_LOG("Gyroscope", "wake up failed", err);

    //measureGyroBias();
    calibrateLevelOffset();

    xTaskCreate(gyroReadTaskEntry, "gyroReadTaskEntry", 2048, gyroscopeHandle, 5, NULL);
}


complimentary_angle_t GyroscopeClass::getComplAngle() {
    complimentary_angle_t angle;
    mpu6050_acce_value_t acce = getAcc();
    mpu6050_gyro_value_t gyro = getGyro();
    err = mpu6050_complimentory_filter(gyroscopeHandle, &acce, &gyro, &angle);
    I2C_ERROR_LOG("Gyroscope", "complimentary filter failed", err);
    return angle;
}

mpu6050_gyro_value_t GyroscopeClass::getGyro() {
    return gyroBuffer[ringBufferIndex];
}

mpu6050_acce_value_t GyroscopeClass::getAcc() {
    if (useImmediateAcc) {
        return getAccImmediate();
    }
    return accBuffer[ringBufferIndex];
}

mpu6050_acce_value_t GyroscopeClass::getAccImmediate() {
    mpu6050_acce_value_t acce = {};
    err = mpu6050_get_acce(gyroscopeHandle, &acce);
    I2C_ERROR_LOG("Gyroscope", "immediate accelerometer read failed", err);

    // matches the Y/Z flip applied in gyroReadTask
    acce.acce_y *= -1;
    acce.acce_z *= -1;
    return acce;
}

bool GyroscopeClass::initialized() const {
    return err == ESP_OK;
}

GyroscopeClass::GroundAngle GyroscopeClass::getGroundAngle() {
    auto acc = getAcc();
    const float x = acc.acce_x - accXBiasG;
    const float y = acc.acce_y - accYBiasG;
    const float z = acc.acce_z - accZBiasG;
    auto rollRad = std::atan2(y, z);
    auto pitchRad = std::atan2(-x, std::sqrt(y * y + z * z));
    return {
        static_cast<float>(rollRad * (180.0f / M_PI)),
        static_cast<float>(pitchRad * (180.0f / M_PI))
    };
}

void GyroscopeClass::gyroReadTaskEntry(void* args) {
    getInstancePtr()->gyroReadTask();
}

void GyroscopeClass::gyroReadTask() {
    int64_t lastTime = esp_timer_get_time();
    while (true) {
        ringBufferIndex = (ringBufferIndex + 1) % GYRO_RING_BUFFER_SIZE;
        mpu6050_get_gyro(gyroscopeHandle, &gyroBuffer[ringBufferIndex]);
        mpu6050_get_acce(gyroscopeHandle, &accBuffer[ringBufferIndex]);

        // The MPU6050 is mounted chip-side down with its X axis still aligned
        // to the nose, i.e. rotated 180 deg about X relative to the sensor's
        // datasheet (chip-up) frame. That rotation leaves X unchanged and
        // flips the sign of Y and Z, so correct it here once for every consumer.
        gyroBuffer[ringBufferIndex].gyro_y *= -1;
        gyroBuffer[ringBufferIndex].gyro_z *= -1;
        accBuffer[ringBufferIndex].acce_y *= -1;
        accBuffer[ringBufferIndex].acce_z *= -1;

        int64_t now = esp_timer_get_time();
        float dt = (now - lastTime) / 1000000.0f;
        lastTime = now;
        yaw += (gyroBuffer[ringBufferIndex].gyro_z - yawGyroBias) * dt; // deg/s * s = deg
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void GyroscopeClass::resetYaw() {
    yaw = 0;
}

float GyroscopeClass::getYawDeg() const {
    return yaw;
}

void GyroscopeClass::measureGyroBias() {
    ESP_LOGI("Gyroscope", "Measuring Yaw Gyro drift");
    float biasZ = 0.0f;

    vTaskDelay(pdMS_TO_TICKS(1000));

    constexpr int measurements = 10000;

    for (int i = 0; i < measurements; i++) {
        mpu6050_gyro_value_t gyro;
        mpu6050_get_gyro(gyroscopeHandle, &gyro);
        biasZ += -gyro.gyro_z; // matches the Y/Z flip applied in gyroReadTask
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    biasZ /= (float)measurements;
    ESP_LOGI("Gyroscope", "Gyroscope bias drift of %f", biasZ);
    yawGyroBias = biasZ;
}


void GyroscopeClass::calibrateLevelOffset() {
    ESP_LOGI("Gyroscope", "Calibrating level offset, keep the airframe still and level");

    constexpr int measurements = 200;
    float xSum = 0.0f;
    float ySum = 0.0f;
    float zSum = 0.0f;

    for (int i = 0; i < measurements; i++) {
        mpu6050_acce_value_t acce;
        mpu6050_get_acce(gyroscopeHandle, &acce);
        // matches the Y/Z flip applied in gyroReadTask
        acce.acce_y *= -1;
        acce.acce_z *= -1;

        xSum += acce.acce_x;
        ySum += acce.acce_y;
        zSum += acce.acce_z;
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    const float measuredXBias = xSum / measurements;
    const float measuredYBias = ySum / measurements;
    // at rest and level, Z should read +1g - the deviation from that is the bias
    const float measuredZBias = (zSum / measurements) - 1.0f;

    // Express the measured offset as the angle it would imply, purely to sanity-check
    // it: a raw-axis offset large enough to look like the airframe was tilted a lot at
    // boot most likely means it actually was tilted - not that the hardware itself has
    // that much zero-g error - so reject and skip calibration rather than baking in a
    // one-off tilt as the new "zero".
    const float impliedRollDeg = std::atan2(measuredYBias, 1.0f) * (180.0f / M_PI);
    const float impliedPitchDeg = std::atan2(-measuredXBias, 1.0f) * (180.0f / M_PI);

    constexpr float MAX_PLAUSIBLE_BIAS_DEG = 45.0f;
    if (std::fabs(impliedRollDeg) > MAX_PLAUSIBLE_BIAS_DEG || std::fabs(impliedPitchDeg) > MAX_PLAUSIBLE_BIAS_DEG) {
        ESP_LOGE("Gyroscope",
                 "Level offset implausible (roll=%.2f deg, pitch=%.2f deg) - airframe was likely not level at boot; skipping calibration",
                 impliedRollDeg, impliedPitchDeg);
        return;
    }

    accXBiasG = measuredXBias;
    accYBiasG = measuredYBias;
    accZBiasG = measuredZBias;
    ESP_LOGI("Gyroscope", "Level offset: x=%.3fg y=%.3fg z=%.3fg (roll=%.2f deg, pitch=%.2f deg)",
             accXBiasG, accYBiasG, accZBiasG, impliedRollDeg, impliedPitchDeg);
}

GyroscopeClass::PlaneAngle GyroscopeClass::getPlaneAngle() {
    return {getGroundAngle(), getYawDeg()};
}
