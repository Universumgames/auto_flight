#include "wifi_helper.hpp"

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define ESP_ERROR_CHECK_SOFT(err) \
    if(err != ESP_OK) { \
        ESP_ERROR_CHECK_WITHOUT_ABORT(err); \
        return err; \
        }

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

const char* TAG_WIFI_HELPER = "wifi_helper";

static wifi_mode_t current_wifi_mode = WIFI_MODE_NULL;
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        auto* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG_WIFI_HELPER, "station join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        auto* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG_WIFI_HELPER, "station leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 50) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_WIFI_HELPER, "retry to connect to the AP");
        }
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG_WIFI_HELPER, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG_WIFI_HELPER, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


esp_err_t prepare_wifi() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK_SOFT(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    return ESP_OK;
}

esp_err_t start_ap() {
    if (current_wifi_mode != WIFI_MODE_NULL) {
        return ESP_FAIL; // Already in a Wi-Fi mode
    }

    esp_err_t err;

    err = prepare_wifi();
    ESP_ERROR_CHECK_SOFT(err);

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    ESP_ERROR_CHECK_SOFT(err);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        nullptr,
        nullptr));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = CONFIG_WIFI_AP_SSID,
            .password = CONFIG_WIFI_AP_PASSWORD,
            .ssid_len = sizeof(CONFIG_WIFI_AP_SSID) - 1,
            .channel = 0,
            .authmode = WIFI_AUTH_WPA2_WPA3_ENTERPRISE,
            .ssid_hidden = false,
        }
    };

    err = esp_wifi_set_mode(WIFI_MODE_AP);
    ESP_ERROR_CHECK_SOFT(err);

    err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    ESP_ERROR_CHECK_SOFT(err);

    err = esp_wifi_start();
    ESP_ERROR_CHECK_SOFT(err);

    current_wifi_mode = WIFI_MODE_AP;

    return ESP_OK;
}

esp_err_t connect_wifi() {
    if (current_wifi_mode != WIFI_MODE_NULL) {
        return ESP_FAIL; // Already in a Wi-Fi mode
    }
    esp_err_t err;

    s_wifi_event_group = xEventGroupCreate();

    err = prepare_wifi();
    ESP_ERROR_CHECK_SOFT(err);

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    ESP_ERROR_CHECK_SOFT(err);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        &instance_got_ip));

    wifi_config_t sta_config = {
        .sta{
            .ssid = CONFIG_WIFI_AP_SSID,
            .password = CONFIG_WIFI_AP_PASSWORD,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = UINT8_MAX,

        }
    };

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_ERROR_CHECK_SOFT(err);

    err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    ESP_ERROR_CHECK_SOFT(err);

    err = esp_wifi_start();
    ESP_ERROR_CHECK_SOFT(err);

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_WIFI_HELPER, "connected to ap SSID:%s password:%s",
                 CONFIG_WIFI_AP_SSID, CONFIG_WIFI_AP_PASSWORD);
    }
    else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG_WIFI_HELPER, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_WIFI_AP_SSID, CONFIG_WIFI_AP_PASSWORD);
    }
    else {
        ESP_LOGE(TAG_WIFI_HELPER, "UNEXPECTED EVENT");
    }

    current_wifi_mode = WIFI_MODE_STA;

    return ESP_OK;
}


esp_err_t init_wifi() {
    if (current_wifi_mode != WIFI_MODE_NULL) {
        return ESP_FAIL; // Already in a Wi-Fi mode
    }

#if CONFIG_WIFI_DEV_MODE
    return connect_wifi();
#else
    return start_ap();
#endif
}

std::string get_ip_address() {
    esp_netif_t* intf = esp_netif_get_default_netif();
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(intf, &ip_info);
    char ip_char[16];
    sprintf(ip_char, IPSTR, IP2STR(&ip_info.ip));
    return std::string{ip_char};
}

#pragma GCC diagnostic pop
