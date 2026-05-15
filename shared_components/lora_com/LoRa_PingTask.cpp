#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include "lora.hpp"
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

        if (lastSendTimeMutex == nullptr || radioMutex == nullptr) {
            continue;
        }

        TickType_t now = xTaskGetTickCount();
        TickType_t timeSinceLastSend = 0;

        // Check time since last send
        if (xSemaphoreTake(lastSendTimeMutex, LORA_PING_CHECK_INTERVAL) == pdTRUE) {
            timeSinceLastSend = now - lastSendTime;
            xSemaphoreGive(lastSendTimeMutex);
        } else {
            continue;
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
            sendRawPacket(pingPacket, nullptr, true);
            ESP_LOGI(TAG_LORA, "PING sent to maintain connection");
            updateLastSendTime();
        }
    }
}

