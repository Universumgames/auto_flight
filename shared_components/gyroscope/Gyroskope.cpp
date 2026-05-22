#include "Gyroskope.hpp"

#include "i2c_manager.hpp"

static GyroscopeClass* gyroscopeInstance = nullptr;

GyroscopeClass& Gyroscope = GyroscopeClass::getInstance();

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
}
