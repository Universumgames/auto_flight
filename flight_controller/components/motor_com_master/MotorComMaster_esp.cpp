#include <bits/codecvt.h>

#include "MotorComMaster.hpp"

#include "FreeRTOSConfig.h"
#include "i2c_manager.hpp"
#include "portmacro.h"
#include "esp_log.h"

#ifndef NATIVE_BUILD

#define ASSERT_VALID_SERVO_VALUE(value) \
    if (value < -100 || value > 100) { \
        ESP_LOGE("MotorCom", "Invalid servo value for " #value " in %d : %d. Must be in range [-100, 100]", __LINE__, value); \
        return false; \
    }

void MotorComMasterClass::init() {
    devHandle = i2c_bus_device_create(I2CManager::getBus(), CONFIG_MOTOR_COM_I2C_ADDRESS, i2c_bus_get_current_clk_speed(I2CManager::getBus()));
}

bool MotorComMasterClass::sendCommand(const ControlCommand command, const int8_t value) {
    ASSERT_VALID_SERVO_VALUE(value);
    lastSentValues[command] = value;

    return sendPacketInternal();
}

bool MotorComMasterClass::sendFullControlPacket(int8_t aileronDiff, int8_t pitch, int8_t thrust, int8_t rudder) {
    ASSERT_VALID_SERVO_VALUE(pitch);
    ASSERT_VALID_SERVO_VALUE(thrust);
    ASSERT_VALID_SERVO_VALUE(rudder);
    ASSERT_VALID_SERVO_VALUE(aileronDiff);
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
        lastSentValues[ControlCommand::RUDDER],
        /// channel 2
        lastSentValues[ControlCommand::PITCH],
        /// channel 3
        lastSentValues[ControlCommand::THRUST],
        /// channel 4
        lastSentValues[ControlCommand::AILERON_DIFF],
    };
    errorCode = (i2c_bus_write_bytes(devHandle, NULL_I2C_MEM_ADDR, 4, reinterpret_cast<uint8_t *>(packet)));
    //I2C_ERROR_LOG("MotorCom", "write packet failed", errorCode);

    return errorCode == ESP_OK;
}

bool MotorComMasterClass::isSlaveConnected() const {
    return errorCode == ESP_OK;
}

bool MotorComMasterClass::isManualOverride() {
    bool isManualOverride;
    errorCode = i2c_bus_read_byte(devHandle, NULL_I2C_MEM_ADDR, (uint8_t*)&isManualOverride);
    //I2C_ERROR_LOG("MotorCom", "read manual override failed", errorCode);
    return errorCode == ESP_OK && isManualOverride;
}

bool MotorComMasterClass::setCommand(ControlCommand command, int8_t value) {
    ASSERT_VALID_SERVO_VALUE(value);
    lastSentValues[command] = value;
    return true;
}

bool MotorComMasterClass::sendFullControlPacket() {
    return sendPacketInternal();
}
#endif
