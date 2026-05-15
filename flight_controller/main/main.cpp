#include <esp_log.h>

#include "GPS_Reader.hpp"
#include "freertos/FreeRTOS.h"
#include "LoRa_Communication.hpp"
#include "serializer.hpp"
#include "route_planner.hpp"

extern "C" int app_main() {

    vTaskDelay(10000 / portTICK_PERIOD_MS); // Wait for 1 second before starting the GPS reader

    GPS_Reader.begin();

    LoRa_Communication.begin();

    return 0;
}
