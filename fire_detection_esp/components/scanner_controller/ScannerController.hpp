#pragma once

#include "driver/i2c_slave.h"

class ScannerControllerClass {

private:
    ScannerControllerClass() = default;
public:
    ~ScannerControllerClass() = delete;
public:
    static ScannerControllerClass *getInstancePtr();
    static ScannerControllerClass& getInstance();
public:
    /**
     * Initialize the I2C slave bus answering to CAMERA_CONTROLLER_I2C_ADDR. Must be called before any other method is used.
     */
    void init();

private:
    i2c_slave_dev_handle_t slaveHandle = nullptr;
};

extern ScannerControllerClass& ScannerController;
