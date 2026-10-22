// scanner_i2c_manager.hpp
// Copy of shared_components/i2c_manager extended with I2C slave support.

#pragma once

#include "i2c_bus.h"
#include "driver/i2c_slave.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifndef I2C_ERROR_LOG
#define I2C_ERROR_LOG(basename, msg, err) if(err != ESP_OK) ESP_LOGE(basename, "%s: %s (%d) (" __FILE_NAME__ ":%d)", msg, esp_err_to_name(err), err, __LINE__)
#endif

class ScannerI2CManager {
public:
    static i2c_bus_handle_t getBus();

    /**
     * Get the I2C slave device answering to the given address, creating it on
     * first use. The slave occupies the same port and pins as the master bus,
     * so only one of getBus() and getSlaveBus() may be used.
     * @param address 7 bit address the slave answers to, only used on first call
     * @return slave device handle, nullptr if it could not be created
     */
    static i2c_slave_dev_handle_t getSlaveBus(uint16_t address);

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
    void initSlave(uint16_t address);

    bool initialized = false;
    i2c_bus_handle_t bus = nullptr;
    bool slaveInitialized = false;
    i2c_slave_dev_handle_t slaveBus = nullptr;
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
};
