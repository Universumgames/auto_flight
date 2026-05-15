#include <cstdio>

#include "esp_log.h"
#include "Frontend.hpp"
#include "GPS_Reader.hpp"
#include "i2c_manager.hpp"
#include "LoRa_Communication.hpp"
#include "wifi_helper.hpp"

extern "C" void app_main(void) {
    auto i2cBus = I2CManager::getBus();
    init_wifi();

    ESP_LOGI("main", "Hello, world!");

    FrontendHandler.init();

    ESP_LOGI("main", "IP Address: %s", get_ip_address().c_str());

    GPS_Reader.begin();

    LoRa_Communication.begin();
}
