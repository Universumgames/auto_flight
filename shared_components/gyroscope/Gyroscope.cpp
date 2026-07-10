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

    xTaskCreate(gyroReadTaskEntry, "gyroReadTaskEntry", 2048, gyroscopeHandle, 5, NULL);
}


complimentary_angle_t GyroscopeClass::getComplAngle() {
    complimentary_angle_t angle;
    mpu6050_acce_value_t acce = accBuffer[ringBufferIndex];
    mpu6050_gyro_value_t gyro = gyroBuffer[ringBufferIndex];
    err = mpu6050_complimentory_filter(gyroscopeHandle, &acce, &gyro, &angle);
    I2C_ERROR_LOG("Gyroscope", "complimentary filter failed", err);
    return angle;
}

mpu6050_gyro_value_t GyroscopeClass::getGyro() {
    return gyroBuffer[ringBufferIndex];
}

mpu6050_acce_value_t GyroscopeClass::getAcc() {
    return accBuffer[ringBufferIndex];
}

bool GyroscopeClass::initialized() const {
    return err == ESP_OK;
}

GyroscopeClass::GroundAngle GyroscopeClass::getGroundAngle() {
    auto acc = getAcc();
    auto roll = std::atan2(acc.acce_y, acc.acce_z);
    auto pitch = std::atan2(-acc.acce_x, std::sqrt(acc.acce_y * acc.acce_y + acc.acce_z * acc.acce_z));
    return {roll, pitch};
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


GyroscopeClass::PlaneAngle GyroscopeClass::getPlaneAngle() {
    auto groundAngle = getGroundAngle();
    // convert ground to deg

    return {
        .roll = static_cast<float>(groundAngle.roll * (180.0f / M_PI)),
        .pitch = static_cast<float>(groundAngle.pitch * (180.0f / M_PI)),
        .yaw = getYawDeg()
    };
}
