// i2c_manager.hpp

#pragma once

#include "i2c_bus.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define I2C_ERROR_LOG(basename, msg, err) if(err != ESP_OK) ESP_LOGE(basename, "%s: %s (%d) (" __FILE_NAME__ ":%d)", msg, esp_err_to_name(err), err, __LINE__)

class I2CManager {
public:
    static i2c_bus_handle_t getBus();

    /**
     * Reset the I2C master peripheral in place after a device reports repeated
     * transaction errors (e.g. NACK storms, timeouts) or is suspected of
     * having desynced the bus. Existing i2c_bus_device_handle_t instances
     * stay valid across this call - only the hardware FSM is reset, the bus
     * object itself is not torn down and recreated.
     */
    static void recoverBus();

private:
    void init();

    bool initialized = false;
    i2c_bus_handle_t bus = nullptr;
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
};