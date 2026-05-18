#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include "mutex_helper.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

constexpr TickType_t LORA_PING_CHECK_INTERVAL = pdMS_TO_TICKS(1000);  // Check every 1 second


void LoRa_CommunicationClass::pingTaskEntry(void* param) {
    auto* instance = static_cast<LoRa_CommunicationClass*>(param);
    instance->pingTaskLoop();
}

void LoRa_CommunicationClass::pingTaskLoop() {
    const TickType_t pingIntervalTicks = pdMS_TO_TICKS(CONFIG_LORA_PING_INTERVAL * 1000);

    while (true) {
        vTaskDelay(LORA_PING_CHECK_INTERVAL);


        TickType_t now = xTaskGetTickCount();
        TickType_t timeSinceLastSend = 0;

        // Check time since last send
        WITH_MUTEX(lastSendTimeMutex){
            timeSinceLastSend = now - lastSendTime;
        }

        // If time exceeded ping interval, send a ping
        if (timeSinceLastSend >= pingIntervalTicks) {
            ESP_LOGD(TAG_LORA, "Sending PING (idle for %ld ms)", timeSinceLastSend / portTICK_PERIOD_MS);

            LoRa_Packet pingPacket = {
                .type = PacketType::PING,
                .messageId = 0,
                .payloadLength = 0,
            };

            // Send ping
            auto success = sendRawPacket(pingPacket, nullptr, true);
            if (success) {
                ESP_LOGI(TAG_LORA, "PING sent to maintain connection");
            }else {
                ESP_LOGE(TAG_LORA, "Failed PING");
            }
            updateLastSendTime();

        }
    }
}

