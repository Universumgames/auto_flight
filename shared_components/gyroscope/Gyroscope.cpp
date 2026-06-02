#include "Gyroscope.hpp"

#include "i2c_manager.hpp"

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
    SOFT_ERROR_CHECK(err);
    err = mpu6050_wake_up(gyroscopeHandle);
    SOFT_ERROR_CHECK(err);
}


complimentary_angle_t GyroscopeClass::getAngle() {
    complimentary_angle_t angle;
    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;
    mpu6050_get_acce(gyroscopeHandle, &acce);
    mpu6050_get_gyro(gyroscopeHandle, &gyro);
    esp_err_t err = mpu6050_complimentory_filter(gyroscopeHandle, &acce, &gyro, &angle);
    ESP_ERROR_CHECK(err);
    return angle;
}

bool GyroscopeClass::initialized() const {
    return err == ESP_OK;
}
