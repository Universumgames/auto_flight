#include <esp_log.h>

#include "FlightController.hpp"
#include "FlightStorage.hpp"
#include "i2c_manager.hpp"
#include "esp_gmf_app_cli.h"

extern "C" int app_main() {
    vTaskDelay(pdMS_TO_TICKS(300));
    I2CManager::getBus();

    esp_gmf_app_cli_init("cmd> ", []{});

    FlightController.init();

    return 0;
}
