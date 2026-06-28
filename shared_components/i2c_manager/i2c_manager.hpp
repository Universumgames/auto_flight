// i2c_manager.hpp

#pragma once

#include "i2c_bus.h"
#include <esp_log.h>

#define I2C_ERROR_LOG(basename, msg, err) if(err != ESP_OK) ESP_LOGE(basename, "%s: %s (%d) (" __FILE_NAME__ ":%d)", msg, esp_err_to_name(err), err, __LINE__)

class I2CManager {
public:
    static i2c_bus_handle_t getBus();

private:
    void init();

    bool initialized = false;
    i2c_bus_handle_t bus = nullptr;
};