#include "MotorComMaster.hpp"
#include <driver/i2c_master.h>

#include "FreeRTOSConfig.h"
#include "portmacro.h"

#ifndef NATIVE_BUILD
void MotorComMasterClass::init() {
    i2c_master_bus_config_t i2c_mst_config = {
        .i2c_port = -1,
        .sda_io_num = (gpio_num_t)CONFIG_MASTER_I2C_PIN_SDA,
        .scl_io_num = (gpio_num_t)CONFIG_MASTER_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        }
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &busHandle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CONFIG_MOTOR_COM_I2C_ADDRESS,
        .scl_speed_hz = CONFIG_MASTER_I2C_FREQUENCY,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(busHandle, &dev_cfg, &devHandle));
}

ControlReturnCode MotorComMasterClass::sendCommand(const ControlCommand command, const uint8_t value) const {
    const uint8_t packet[2] = {static_cast<uint8_t>(command), value};
    uint8_t response;

    ESP_ERROR_CHECK(i2c_master_transmit_receive(devHandle, packet, sizeof(packet), &response, sizeof(uint8_t), -1));

    return static_cast<ControlReturnCode>(response);
}

uint8_t MotorComMasterClass::readValue(const ControlCommand command) const {
    const auto cmd = static_cast<uint8_t>(command);
    uint8_t response;

    ESP_ERROR_CHECK(i2c_master_transmit_receive(devHandle, &cmd, sizeof(cmd), &response, sizeof(response), -1));

    return response;
}

bool MotorComMasterClass::isSlaveConnected() const {
    esp_err_t errorCode = i2c_master_probe(busHandle, CONFIG_MOTOR_COM_I2C_ADDRESS, 100 / portTICK_PERIOD_MS);
    return errorCode == ESP_OK;
}
#endif
