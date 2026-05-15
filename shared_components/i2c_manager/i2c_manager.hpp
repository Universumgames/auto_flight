// i2c_manager.hpp

#pragma once

#include "driver/i2c_master.h"

class I2CManager {
public:
    static i2c_master_bus_handle_t getBus();

private:
    static void init();

    static inline bool initialized = false;
    static inline i2c_master_bus_handle_t bus = nullptr;
};