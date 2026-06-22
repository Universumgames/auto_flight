#include "MotorComMaster.hpp"

#include "FreeRTOSConfig.h"
#include "i2c_manager.hpp"
#include "portmacro.h"

#ifndef NATIVE_BUILD
void MotorComMasterClass::init() {
    devHandle = i2c_bus_device_create(I2CManager::getBus(), CONFIG_MOTOR_COM_I2C_ADDRESS, i2c_bus_get_current_clk_speed(I2CManager::getBus()));
}

bool MotorComMasterClass::sendCommand(const ControlCommand command, const int8_t value) {
    lastSentValues[command] = value;


    return sendPacketInternal();
}

bool MotorComMasterClass::sendFullControlPacket(int8_t aileronDiff, int8_t pitch, int8_t thrust, int8_t rudder) {
    lastSentValues[ControlCommand::AILERON_DIFF] = aileronDiff;
    lastSentValues[ControlCommand::PITCH] = pitch;
    lastSentValues[ControlCommand::THRUST] = thrust;
    lastSentValues[ControlCommand::RUDDER] = rudder;
    return sendPacketInternal();
}

bool MotorComMasterClass::sendPacketInternal() {
    int8_t packet[4] = {
        /// from futba my RC remote, order defined via the servo connections on the plane
        /// channel 1
        lastSentValues[ControlCommand::AILERON_DIFF],
        /// channel 2
        lastSentValues[ControlCommand::PITCH],
        /// channel 3
        lastSentValues[ControlCommand::THRUST],
        /// channel 4
        lastSentValues[ControlCommand::RUDDER],
    };
    esp_err_t errorCode = (i2c_bus_write_bytes(devHandle, NULL_I2C_MEM_ADDR, sizeof(packet), reinterpret_cast<uint8_t *>(packet)));

    return errorCode == ESP_OK;
}

bool MotorComMasterClass::isSlaveConnected() const {
    uint8_t response;
    esp_err_t errorCode = i2c_bus_read_byte(devHandle, NULL_I2C_MEM_ADDR, &response);
    return errorCode == ESP_OK;
}
#endif
