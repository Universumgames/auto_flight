#pragma once
#ifndef NATIVE_BUILD
#include <driver/i2c_master.h>
#endif
#include "MotorCommunication.hpp"

class MotorComMasterClass {

private:
    MotorComMasterClass() = default;
public:
    ~MotorComMasterClass() = delete;
public:
    static MotorComMasterClass *getInstancePtr();
    static MotorComMasterClass& getInstance();
public:
    /**
     * Initialize I2C connection to slave. Must be called before any other method is used.
     */
    void init();

    /**
     * Check if slave is connected and responsive
     * @return true if slave responds 
     */
    [[nodiscard]] bool isSlaveConnected() const;



private:
    /**
     * Send a command to the slave
     * @param command command to send
     * @param value the value
     * @return return code of slave
     */
    ControlReturnCode sendCommand(ControlCommand command, uint8_t value) const;

    /**
     * Read a value from the slave
     * @param command command to read
     * @return value read from slave
     */
    uint8_t readValue(ControlCommand command) const;

private:
#ifndef NATIVE_BUILD
    i2c_master_bus_handle_t busHandle;
    i2c_master_dev_handle_t devHandle;
#endif
};

extern MotorComMasterClass& motorComMaster;