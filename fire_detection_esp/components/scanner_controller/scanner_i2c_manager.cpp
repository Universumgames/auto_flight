// scanner_i2c_manager.cpp
// Copy of shared_components/i2c_manager extended with I2C slave support.

#include "scanner_i2c_manager.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include "esp_rom_sys.h"

static ScannerI2CManager* instance = new ScannerI2CManager();

// A slave whose hardware TWI got desynchronized by a glitch (e.g. during the
// ESP32's own pin muxing for the I2C peripheral) doesn't necessarily hold SDA
// low at rest - it can sit electrically idle and still corrupt later,
// unrelated transactions to other devices. STOP-condition detection is
// dedicated comparator hardware on AVR TWI, independent of its internal bit
// counter, so unconditionally emitting a STOP (plus a few clocks in case a
// slave *is* holding SDA low) resyncs it even when nothing looks stuck.
// See the I2C bus recovery procedure, NXP UM10204 sec. 3.1.16.
static void recoverI2CBus(gpio_num_t sda, gpio_num_t scl) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sda) | (1ULL << scl),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(scl, 1);
    gpio_set_level(sda, 1);
    esp_rom_delay_us(5);

    if (gpio_get_level(sda) == 0) {
        ESP_LOGW("scanner_i2c_manager", "SDA held low on bus init, clocking it free");
        for (int i = 0; i < 9 && gpio_get_level(sda) == 0; i++) {
            gpio_set_level(scl, 0);
            esp_rom_delay_us(5);
            gpio_set_level(scl, 1);
            esp_rom_delay_us(5);
        }
    }

    // Unconditionally issue a manual STOP condition (SDA rises while SCL is
    // high) to resync any slave left mid-transaction, even one that isn't
    // visibly holding a line low right now.
    gpio_set_level(sda, 0);
    esp_rom_delay_us(5);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(5);
    gpio_set_level(sda, 1);
    esp_rom_delay_us(5);

    if (gpio_get_level(sda) == 0) {
        ESP_LOGE("scanner_i2c_manager", "I2C bus recovery failed, SDA still stuck low");
    } else {
        ESP_LOGI("scanner_i2c_manager", "I2C bus recovery STOP sent");
    }
}

i2c_bus_handle_t ScannerI2CManager::getBus() {
    if (instance->slaveInitialized) {
        ESP_LOGE("scanner_i2c_manager", "I2C port already in use as slave, cannot create master bus");
        return nullptr;
    }

    if (!instance->initialized) {
        instance->init();
    }

    return instance->bus;
}

i2c_slave_dev_handle_t ScannerI2CManager::getSlaveBus(uint16_t address) {
    if (instance->initialized) {
        ESP_LOGE("scanner_i2c_manager", "I2C port already in use as master, cannot create slave bus");
        return nullptr;
    }

    if (!instance->slaveInitialized) {
        instance->initSlave(address);
    }

    return instance->slaveBus;
}

void ScannerI2CManager::init() {
    ESP_LOGI("scanner_i2c_manager", "Initializing I2C bus");

    static i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (gpio_num_t)CONFIG_I2C_PIN_SDA,
        .scl_io_num = (gpio_num_t)CONFIG_I2C_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 100000,
        },
        .clk_flags = 0,
    };

    vTaskDelay(pdMS_TO_TICKS(100));  // let devices finish power-on reset before bus creation

    recoverI2CBus((gpio_num_t)CONFIG_I2C_PIN_SDA, (gpio_num_t)CONFIG_I2C_PIN_SCL);

    bus = i2c_bus_create(I2C_NUM_0, &conf);

    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI("scanner_i2c_manager", "I2C bus created, SDA=%d SCL=%d (raw pin level, 0 = stuck low)",
             gpio_get_level((gpio_num_t)CONFIG_I2C_PIN_SDA),
             gpio_get_level((gpio_num_t)CONFIG_I2C_PIN_SCL));

    uint8_t found[16] = {};
    uint8_t count = i2c_bus_scan(bus, found, 16);
    ESP_LOGI("scanner_i2c_manager", "I2C scan: %d device(s) found", count);
    for (uint8_t i = 0; i < count; i++) {
        ESP_LOGI("scanner_i2c_manager", "  found device at 0x%02X", found[i]);
    }

    ESP_LOGI("scanner_i2c_manager", "waiting for devices to boot");
    vTaskDelay(pdMS_TO_TICKS(2000));

    initialized = true;
}

void ScannerI2CManager::initSlave(uint16_t address) {
    ESP_LOGI("scanner_i2c_manager", "Initializing I2C slave bus at address 0x%02X", address);

    i2c_slave_config_t conf = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t)CONFIG_I2C_PIN_SDA,
        .scl_io_num = (gpio_num_t)CONFIG_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .send_buf_depth = 256,
        .receive_buf_depth = 256,
        .slave_addr = address,
        .addr_bit_len = I2C_ADDR_BIT_LEN_7,
        .intr_priority = 0,
        .flags = {
            .allow_pd = false,
            .enable_internal_pullup = true,
#if SOC_I2C_SLAVE_SUPPORT_BROADCAST
            .broadcast_en = false,
#endif
        },
    };

    esp_err_t err = i2c_new_slave_device(&conf, &slaveBus);
    I2C_ERROR_LOG("scanner_i2c_manager", "i2c_new_slave_device failed", err);
    if (err != ESP_OK) {
        slaveBus = nullptr;
        return;
    }

    ESP_LOGI("scanner_i2c_manager", "I2C slave bus created, SDA=%d SCL=%d",
             CONFIG_I2C_PIN_SDA, CONFIG_I2C_PIN_SCL);

    slaveInitialized = true;
}

void ScannerI2CManager::recoverBus() {
    if (!instance->initialized || !instance->bus) {
        return;
    }

    ESP_LOGW("scanner_i2c_manager", "Resetting I2C master peripheral after repeated transaction errors");

    i2c_master_bus_handle_t rawBus = i2c_bus_get_internal_bus_handle(instance->bus);
    if (!rawBus) {
        ESP_LOGE("scanner_i2c_manager", "Cannot recover: no underlying master bus handle");
        return;
    }

    // i2c_master_bus_reset() does not take the driver's own internal bus
    // lock, so it can otherwise land in the middle of a transaction issued
    // from another task (e.g. the gyroscope's dedicated read task).
    esp_err_t err = i2c_master_bus_reset(rawBus);
    I2C_ERROR_LOG("scanner_i2c_manager", "i2c_master_bus_reset failed", err);
}
