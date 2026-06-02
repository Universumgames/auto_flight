#include <esp_log.h>

#include "FlightController.hpp"
#include "FlightStorage.hpp"
#include "i2c_manager.hpp"

extern "C" int app_main() {
    I2CManager::getBus();

    FlightController.init();

    return 0;
}
