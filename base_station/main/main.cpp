#include <cstdio>

#include "esp_log.h"
#include "Frontend.hpp"
#include "wifi_helper.hpp"

extern "C" void app_main(void) {
    init_wifi();

    ESP_LOGI("main", "Hello, world!");

    FrontendHandler.init();

    ESP_LOGI("main", "IP Address: %s", get_ip_address().c_str());
}
