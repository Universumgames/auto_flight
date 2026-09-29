#include "BaseController.hpp"
#include "esp_gmf_app_cli.h"
#include "esp_log.h"

static void checkSystemStats(void* param) {
    char buffer[2048];
    while (true) {
        vTaskGetRunTimeStats(buffer);
        printf("%s\n", buffer);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

extern "C" void app_main(void) {
    //xTaskCreate(checkSystemStats, "SystemStatsTask", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);

    esp_gmf_app_cli_init("cmd> ", []{});

    BaseController.init();
}
