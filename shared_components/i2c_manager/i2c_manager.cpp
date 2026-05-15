// i2c_manager.cpp

#include "i2c_manager.hpp"

i2c_master_bus_handle_t I2CManager::getBus() {
    if (!initialized) {
        init();
    }

    return bus;
}

void I2CManager::init() {
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t) CONFIG_I2C_PIN_SDA,
        .scl_io_num = (gpio_num_t) CONFIG_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));

    initialized = true;
}