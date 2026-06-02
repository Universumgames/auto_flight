#pragma once
#include "mpu6050.h"

class GyroscopeClass {
private:
    GyroscopeClass() = default;

public:
    ~GyroscopeClass() = delete;

    static GyroscopeClass& getInstance();
    static GyroscopeClass* getInstancePtr();

    void begin();

    bool initialized() const;

    complimentary_angle_t getAngle();

private:
    mpu6050_handle_t gyroscopeHandle;
    esp_err_t err = ESP_FAIL;
};


extern GyroscopeClass& Gyroscope;
