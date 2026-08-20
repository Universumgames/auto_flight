// Template for a second, independent FreeRTOS task doing I2C sensor work
// alongside inference (see main.cpp). Uses ESP-IDF's newer i2c_master driver
// (driver/i2c_master.h, requires ESP-IDF >= 5.2 -- see main/idf_component.yml).
//
// This is not a real sensor driver: read_register()/SENSOR_I2C_ADDR are
// placeholders. Point them at your actual device and adjust the read
// protocol (some sensors need multi-byte commands, delays between write
// and read, CRC checks, etc -- read_register() below is the generic
// "write register address, then read N bytes" shape most simple sensors
// use, e.g. BME280-style).
#include "i2c_task.hpp"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace {
const char *TAG = "i2c_task";

// Adjust to your board's wiring.
constexpr int kI2cSdaGpio = 21;
constexpr int kI2cSclGpio = 22;
constexpr uint32_t kI2cClockHz = 100000;

// Placeholder device address/register -- replace with your actual sensor's.
constexpr uint8_t kSensorI2cAddr = 0x76;
constexpr uint8_t kSensorRegId = 0x00;

i2c_master_bus_handle_t s_bus = nullptr;
i2c_master_dev_handle_t s_dev = nullptr;
SemaphoreHandle_t s_lock = nullptr;
uint8_t s_latest_value = 0;
bool s_have_reading = false;

esp_err_t i2c_bus_init() {
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = static_cast<gpio_num_t>(kI2cSdaGpio);
    bus_config.scl_io_num = static_cast<gpio_num_t>(kI2cSclGpio);
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus);
    if (err != ESP_OK) {
        return err;
    }

    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = kSensorI2cAddr;
    dev_config.scl_speed_hz = kI2cClockHz;

    return i2c_master_bus_add_device(s_bus, &dev_config, &s_dev);
}

esp_err_t read_register(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

void i2c_task_main(void *arg) {
    (void)arg;
    while (true) {
        uint8_t value = 0;
        esp_err_t err = read_register(kSensorRegId, &value, 1);
        if (err == ESP_OK) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_latest_value = value;
            s_have_reading = true;
            xSemaphoreGive(s_lock);
        } else {
            ESP_LOGW(TAG, "read_register failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
}  // namespace

esp_err_t i2c_task_start() {
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_bus_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // A FreeRTOS task, not a pthread -- ESP-IDF's normal concurrency unit.
    // Pinned to core 0 so it doesn't compete with the heavier inference
    // task (see main.cpp, pinned to core 1) for CPU time. If you'd rather
    // write standard C++ std::thread/std::mutex code instead of FreeRTOS
    // primitives, ESP-IDF's pthread component (esp_pthread) maps those
    // onto FreeRTOS tasks for you -- same underlying mechanism either way.
    BaseType_t ok = xTaskCreatePinnedToCore(i2c_task_main, "i2c_task", 4096, nullptr,
                                             /*priority=*/5, nullptr, /*core=*/0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool i2c_task_get_latest(uint8_t *value_out) {
    if (!s_lock) {
        return false;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool have = s_have_reading;
    if (have) {
        *value_out = s_latest_value;
    }
    xSemaphoreGive(s_lock);
    return have;
}
