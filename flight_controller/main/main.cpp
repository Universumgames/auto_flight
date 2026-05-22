#include <esp_log.h>

#include "Barometer.hpp"
#include "GPS_Reader.hpp"
#include "i2c_manager.hpp"
#include "freertos/FreeRTOS.h"
#include "LoRa_Communication.hpp"
#include "serializer.hpp"
#include "route_planner.hpp"

extern "C" int app_main() {

    vTaskDelay(10000 / portTICK_PERIOD_MS); // Wait for 1 second before starting the GPS reader

    GPS_Reader.begin();

    LoRa_Communication.begin();

    I2CManager::getBus(); // initialize I2C bus

    Barometer.begin();


    while (true) {
        ESP_LOGI("main", "Current Pressure %f hPa", Barometer.getPressure());
        ESP_LOGI("main", "Current Altitude %f m", Barometer.getEstimatedAltitude());
        ESP_LOGI("main", "Current Temperature %f C", Barometer.getTemperature());
        ESP_LOGI("main", "Current Humidity %f %%", Barometer.getHumidity());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    return 0;
}
