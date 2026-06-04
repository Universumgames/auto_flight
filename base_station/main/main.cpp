#include "BaseController.hpp"
#include "esp_log.h"
#include "wifi_helper.hpp"

static void checkSystemStats(void* param) {
    char buffer[2048];
    while (true) {
        vTaskGetRunTimeStats(buffer);
        printf("%s\n", buffer);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

extern "C" void app_main(void) {
    init_wifi();

    ESP_LOGI("main", "IP Address: %s", get_ip_address().c_str());

    //xTaskCreate(checkSystemStats, "SystemStatsTask", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);

    BaseController.init();
}
