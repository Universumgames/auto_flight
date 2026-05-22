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

private:
    mpu6050_handle_t gyroscopeHandle;
};


extern GyroscopeClass& Gyroscope;
