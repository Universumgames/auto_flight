#pragma once
#include <string>

#include "esp_err.h"

/**
 * Starts the Access Point according to the configuration
 * @return ESP_OK on success, otherwise the error codes provided by esp idf
 */
esp_err_t start_ap();

#if CONFIG_WIFI_DEV_MODE
/**
 * Connect to existing network
 * @return ESP_OF on success, otherwise the error codes provided by esp idf
 */
esp_err_t connect_wifi();
#endif

/**
 * Start and initialize wifi according to config.
 * @return ESP_OK on success, otherwise the error codes provided by esp idf
 */
esp_err_t init_wifi();

std::string get_ip_address();