// i2c_manager.hpp

#pragma once

#include "i2c_bus.h"

class I2CManager {
public:
    static i2c_bus_handle_t getBus();

private:
    void init();

    bool initialized = false;
    i2c_bus_handle_t bus = nullptr;
};