#warning "Flight_controller_main"

#include <esp_log.h>

#include "GPS_Reader.hpp"
#include "freertos/FreeRTOS.h"
#include <lora.h>
#include "LoRa_Communication.hpp"
#include "serializer.hpp"
#include "route_planner.hpp"

extern "C" int app_main() {

    vTaskDelay(10000 / portTICK_PERIOD_MS); // Wait for 1 second before starting the GPS reader

    auto* gps_reader = new GPS_Reader();

    gps_reader->begin();

    float x,y,z;
    while (true) {
        gps_reader->getCurrentCoordinates(x,y,z);
        ESP_LOGI("main", "%f, %f, %f", x,y,z);
        vTaskDelay(100/portTICK_PERIOD_MS);
    }

    return 0;
}
