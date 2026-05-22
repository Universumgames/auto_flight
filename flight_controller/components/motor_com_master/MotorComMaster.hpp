#pragma once
#ifndef NATIVE_BUILD
#include "i2c_bus.h"
#endif
#include <unordered_map>

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
     * @return true on success, false otherwise
     */
    bool sendCommand(ControlCommand command, uint8_t value);

private:
#ifndef NATIVE_BUILD
    i2c_bus_device_handle_t devHandle = nullptr;
#endif

    std::unordered_map<ControlCommand, uint8_t> lastSentValues;
};

extern MotorComMasterClass& motorComMaster;