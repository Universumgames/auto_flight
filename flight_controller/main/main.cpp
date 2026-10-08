#include <esp_log.h>

#include "esp_console.h"
#include "FlightController.hpp"
#include "FlightStorage.hpp"
#include "i2c_manager.hpp"
#include "esp_gmf_app_cli.h"

static const esp_console_cmd_t cmds[] = {
    {
        .command = "connection",
        .help = "Prints the connection status of all components",
        .func = [](int argc, char **argv) {
            printf("Connection status of all components:\n");
            printf("\tGPS: %s\n", FlightStorage.getPlaneGPSConnectionState() == ConnectionState::CONNECTED ? "Connected" : "Disconnected");
            printf("\tMotor Control: %s\n", FlightStorage.getPlaneMotorControlConnectionState() == ConnectionState::CONNECTED ? "Connected" : "Disconnected");
            printf("\tManual Override: %s\n", FlightStorage.getPlaneManualOverride() ? "Active" : "Inactive");
            printf("\tMagnetometer: %s\n", FlightStorage.getPlaneMagnetometerConnectionState() == ConnectionState::CONNECTED ? "Connected" : "Disconnected");
            printf("\tAccelerometer: %s\n", FlightStorage.getPlaneAccelerometerConnectionState() == ConnectionState::CONNECTED ? "Connected" : "Disconnected");
            printf("\tBarometer: %s\n", FlightStorage.getPlaneBarometerConnectionState() == ConnectionState::CONNECTED ? "Connected" : "Disconnected");
            printf("\tBattery: %s\n", FlightStorage.getPlaneBatteryConnectionState() == ConnectionState::CONNECTED ? "Connected" : "Disconnected");
            return 0;
        },
    }
};

extern "C" int app_main() {
    vTaskDelay(pdMS_TO_TICKS(300));
    I2CManager::getBus();

    esp_gmf_app_cli_init("cmd> ", [] {
        for (const auto& cmd : cmds) {
            ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
        }
    });

    FlightController.init();

    return 0;
}
