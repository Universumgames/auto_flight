#pragma once
#include "mpu6050.h"

#define GYRO_RING_BUFFER_SIZE (10)

class GyroscopeClass {
private:
    explicit GyroscopeClass(bool useImmediateAcc = true): useImmediateAcc(useImmediateAcc) {}

public:
    ~GyroscopeClass() = delete;

    // useImmediateAcc only takes effect on the very first call, when the singleton
    // is constructed; later calls just return the already-created instance.
    static GyroscopeClass& getInstance();
    static GyroscopeClass* getInstancePtr();

    void begin();

    [[nodiscard]] bool initialized() const;

    // roll/pitch in degrees, with the boot-time level-calibration bias
    // already removed - 0 means "level as calibrated", not "raw IMU zero".
    struct GroundAngle {
        float roll, pitch;
    };

    struct PlaneAngle: GroundAngle {
        float yaw;
    };

    complimentary_angle_t getComplAngle();
    mpu6050_gyro_value_t getGyro();

    // Returns the most recent ring-buffer sample, or a fresh immediate reading
    // if useImmediateAcc was set on construction - see getAccImmediate().
    mpu6050_acce_value_t getAcc();

    // Reads the accelerometer directly off the bus, bypassing the ring buffer
    // filled by the background task, so the caller always gets a fresh sample
    // at the cost of blocking for the I2C transaction.
    mpu6050_acce_value_t getAccImmediate();

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
    // Raw accelerometer zero-g offsets (in g), measured at boot. Subtracted from
    // acce_x/y/z before atan2 so the correction holds across the whole range of
    // motion, not just near level - a per-axis offset shifts the atan2
    // zero-crossing without moving where it saturates towards +-90 deg, so
    // correcting the angle after the fact (a constant deg offset) only works
    // as an approximation close to level.
    float accXBiasG = 0;
    float accYBiasG = 0;
    float accZBiasG = 0;
    bool useImmediateAcc;

    static void gyroReadTaskEntry(void* args);

    [[noreturn]] void gyroReadTask();

    void measureGyroBias();

    // Averages raw accelerometer readings over a short window at boot so that
    // whatever attitude the airframe is resting at becomes the zero point. Without
    // this, any IMU mounting misalignment or per-axis zero-g offset shows up as a
    // permanent roll/pitch offset that the controller reads as "not level" even
    // when the plane physically is.
    void calibrateLevelOffset();
};


extern GyroscopeClass& Gyroscope;
