// scanner_i2c_manager.hpp
// Copy of shared_components/i2c_manager extended with I2C slave support.

#pragma once

#include "i2c_bus.h"
#include "driver/i2c_slave.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <cstddef>
#include <cstdint>
#include <functional>

#ifndef I2C_ERROR_LOG
#define I2C_ERROR_LOG(basename, msg, err) if(err != ESP_OK) ESP_LOGE(basename, "%s: %s (%d) (" __FILE_NAME__ ":%d)", msg, esp_err_to_name(err), err, __LINE__)
#endif

class ScannerI2CManager {
public:
    // Maximum number of bytes of a single master write handed to the receive callback, longer writes are truncated
    static constexpr size_t SLAVE_RX_MAX_LENGTH = 64;

    /**
     * Called from the slave task (not ISR context) after the master wrote data
     * @param data the bytes written by the master, data[0] is the command byte
     * @param length number of valid bytes in data, at least 1
     */
    using SlaveReceiveCallback = std::function<void(const uint8_t* data, size_t length)>;

    /**
     * Called from the slave task (not ISR context) when the master wants to read while the slave fifo is empty.
     * The master is clock stretched until data is written via i2c_slave_write.
     * @param command the first byte of the last write by the master
     */
    using SlaveRequestCallback = std::function<void(uint8_t command)>;

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
     * Register the callbacks handling master writes and reads on the slave bus and start the task dispatching them.
     * getSlaveBus() must have been called successfully before. Can only be called once.
     * @param onReceive called after the master wrote data
     * @param onRequest called when the master wants to read data
     * @return true on success, false otherwise
     */
    static bool registerSlaveCallbacks(SlaveReceiveCallback onReceive, SlaveRequestCallback onRequest);

    /**
     * Reset the I2C master peripheral in place after a device reports repeated
     * transaction errors (e.g. NACK storms, timeouts) or is suspected of
     * having desynced the bus. Existing i2c_bus_device_handle_t instances
     * stay valid across this call - only the hardware FSM is reset, the bus
     * object itself is not torn down and recreated.
     */
    static void recoverBus();

private:
    struct SlaveEvent {
        enum Type : uint8_t {
            RECEIVE, // master wrote data, stored in data/length
            REQUEST // master wants to read, command is the last command byte the master wrote
        } type;
        uint8_t command;
        uint32_t length;
        uint8_t data[SLAVE_RX_MAX_LENGTH];
    };

    void init();
    void initSlave(uint16_t address);

    static void slaveTaskLoop(void* arg);

    /**
     * @brief Callback signature for I2C slave.
     *
     * @param[in]  i2c_slave Handle for I2C slave.
     * @param[out] evt_data I2C capture event data, fed by driver
     * @param[in]  arg User data, set in `i2c_slave_register_event_callbacks()`
     *
     * @return Whether a high priority task has been waken up by this function
     */
    static bool on_slave_receive(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_rx_done_event_data_t* evt_data,
                                 void* arg);

    /**
     * @brief Callback signature for I2C slave request event. When this callback is triggered that means master want to read data
     * from slave while there is no data in slave fifo. So user should write data to fifo via `i2c_slave_write`
     *
     * @param[in]  i2c_slave Handle for I2C slave.
     * @param[out] evt_data I2C receive event data, fed by driver
     * @param[in]  arg User data, set in `i2c_slave_register_event_callbacks()`
     *
     * @return Whether a high priority task has been waken up by this function
     */
    static bool on_slave_request(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_request_event_data_t* evt_data,
                                 void* arg);

    bool initialized = false;
    i2c_bus_handle_t bus = nullptr;
    bool slaveInitialized = false;
    i2c_slave_dev_handle_t slaveBus = nullptr;

    QueueHandle_t slaveEventQueue = nullptr;
    SlaveReceiveCallback slaveReceiveCallback;
    SlaveRequestCallback slaveRequestCallback;
    // Last command byte written by the master, handed to the request callback on the following read
    volatile uint8_t lastCommand = 0;
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
};
