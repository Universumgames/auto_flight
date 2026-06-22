#pragma once
#include "mpu6050.h"

#define GYRO_RING_BUFFER_SIZE (10)

class GyroscopeClass {
private:
    GyroscopeClass() = default;

public:
    ~GyroscopeClass() = delete;

    static GyroscopeClass& getInstance();
    static GyroscopeClass* getInstancePtr();

    void begin();

    [[nodiscard]] bool initialized() const;

    struct GroundAngle {
        float roll, pitch;
    };

    struct PlaneAngle {
        float roll, pitch, yaw;
    };

    complimentary_angle_t getComplAngle();
    mpu6050_gyro_value_t getGyro();
    mpu6050_acce_value_t getAcc();

    GroundAngle getGroundAngle();
    PlaneAngle getPlaneAngle();

    void resetYaw();

    [[nodiscard]] float getYawDeg() const;

private:
    mpu6050_handle_t gyroscopeHandle;
    esp_err_t err = ESP_FAIL;

    mpu6050_gyro_value_t gyroBuffer[GYRO_RING_BUFFER_SIZE] = {};
    mpu6050_acce_value_t accBuffer[GYRO_RING_BUFFER_SIZE] = {};
    size_t ringBufferIndex = 0;
    float yaw = 0;
    float yawGyroBias = 0;

    static void gyroReadTaskEntry(void* args);

    [[noreturn]] void gyroReadTask();

    void measureGyroBias();
};


extern GyroscopeClass& Gyroscope;
