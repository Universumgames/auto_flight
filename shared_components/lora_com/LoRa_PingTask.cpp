#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include "mutex_helper.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

void LoRa_CommunicationClass::pingTaskEntry(void* param) {
    auto* instance = static_cast<LoRa_CommunicationClass*>(param);
    instance->pingTaskLoop();
}

void LoRa_CommunicationClass::pingTaskLoop() {
    while (true) {
        vTaskDelay(LORA_PING_CHECK_INTERVAL);


        time_t now = time(nullptr);
        time_t timeSinceLastSend = 0;

        // Check time since last send
        WITH_MUTEX(lastSendTimeMutex){
            timeSinceLastSend = now - lastSendTime;
        }

        // If time exceeded ping interval, send a ping
        if (timeSinceLastSend >= CONFIG_LORA_PING_INTERVAL) {
            ESP_LOGD(TAG_LORA, "Sending PING (idle for %ld ms)", timeSinceLastSend);

            LoRa_Packet_Internal pingPacket = {
                .type = PacketType::PING,
                .messageId = nextMessageId.fetch_add(1),
                .payloadLength = 0,
            };

            // Send ping
            bool success = false;
            WITH_MUTEX(sendMutex) {
                success = sendRawPacket(pingPacket, nullptr, true);
            }
            if (success) {
                ESP_LOGI(TAG_LORA, "PING sent to maintain connection");
            }else {
                ESP_LOGE(TAG_LORA, "Failed PING");
            }
            updateLastSendTime();
        }
    }
}

