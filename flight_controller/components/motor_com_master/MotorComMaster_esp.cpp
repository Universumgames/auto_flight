#include "MotorComMaster.hpp"

#include "FreeRTOSConfig.h"
#include "i2c_manager.hpp"
#include "portmacro.h"

#ifndef NATIVE_BUILD
void MotorComMasterClass::init() {
    devHandle = i2c_bus_device_create(I2CManager::getBus(), CONFIG_MOTOR_COM_I2C_ADDRESS, i2c_bus_get_current_clk_speed(I2CManager::getBus()));
}

bool MotorComMasterClass::sendCommand(const ControlCommand command, const uint8_t value) {
    lastSentValues[command] = value;
    uint8_t packet[4] = {
        // servo value 1,
        // servo value 2,
        // servo value 3,
        // servo value 4,
    };

    esp_err_t errorCode = (i2c_bus_write_bytes(devHandle, NULL_I2C_MEM_ADDR, sizeof(packet), packet));

    return errorCode == ESP_OK ? ESP_OK : ESP_FAIL;
}

bool MotorComMasterClass::isSlaveConnected() const {
    uint8_t response;
    esp_err_t errorCode = i2c_bus_read_byte(devHandle, NULL_I2C_MEM_ADDR, &response);
    return errorCode == ESP_OK;
}
#endif
