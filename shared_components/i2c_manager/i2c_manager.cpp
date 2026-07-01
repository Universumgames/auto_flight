// i2c_manager.cpp

#include "i2c_manager.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

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
            .clk_speed = 400000,
        }
    };

    vTaskDelay(pdMS_TO_TICKS(100));  // let devices finish power-on reset before bus creation

    bus = i2c_bus_create(I2C_NUM_0, &conf);

    i2c_master_bus_handle_t master_handle = i2c_bus_get_internal_bus_handle(bus);
    if (master_handle) {
        i2c_master_bus_reset(master_handle);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI("i2c_manager", "I2C bus created");

    uint8_t found[8] = {};
    uint8_t count = i2c_bus_scan(bus, found, sizeof(found));
    ESP_LOGI("i2c_manager", "I2C scan: %d device(s) found", count);

    initialized = true;
}
