// i2c_manager.cpp

#include "i2c_manager.hpp"

#include "esp_log.h"

static I2CManager* instance = new I2CManager();

i2c_bus_handle_t I2CManager::getBus() {
    if (!instance->initialized) {
        instance->init();
    }

    return instance->bus;
}

void I2CManager::init() {
    ESP_LOGI("i2c_manager", "Initializing I2C bus");

    static i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (gpio_num_t)CONFIG_I2C_PIN_SDA,
        .scl_io_num = (gpio_num_t)CONFIG_I2C_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 100000,
        }
    };

    bus = i2c_bus_create(I2C_NUM_0, &conf);

    ESP_LOGI("i2c_manager", "I2C bus created");
    initialized = true;
}
