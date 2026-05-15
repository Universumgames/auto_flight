#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include "lora.hpp"
#include <algorithm>
#include <cstring>
#include <memory>

#include "driver/gpio.h"

#include "FlightStorage.hpp"
#include "helper.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

constexpr size_t LORA_MAX_PACKET_SIZE = 255;

constexpr TickType_t LORA_RX_POLL_DELAY = pdMS_TO_TICKS(10);
constexpr TickType_t LORA_PAYLOAD_TIMEOUT = pdMS_TO_TICKS(3000);

void LoRa_CommunicationClass::receiveTaskEntry(void* param) {
    auto* instance = static_cast<LoRa_CommunicationClass*>(param);
    instance->receiveTaskLoop();
}

void LoRa_CommunicationClass::sendAckPacketInternal(uint8_t messageId) {
    if (radioMutex == nullptr) {
        return;
    }

    if (xSemaphoreTake(radioMutex, LORA_RX_POLL_DELAY) != pdTRUE) {
        return;
    }

    LoRa_Packet ackPacket{};
    ackPacket.type = PacketType::ACK;
    ackPacket.messageId = messageId;
    ackPacket.payloadLength = 0;
    lora_send_packet(reinterpret_cast<uint8_t*>(&ackPacket), sizeof(ackPacket));
    lora_receive();
    xSemaphoreGive(radioMutex);
}

int LoRa_CommunicationClass::waitForPayload(uint8_t* dataBuffer, uint8_t expectedSize) {
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < LORA_PAYLOAD_TIMEOUT) {
        if (radioMutex == nullptr) {
            return -1;
        }

        if (xSemaphoreTake(radioMutex, LORA_RX_POLL_DELAY) == pdTRUE) {
            int payloadSize = 0;
            if (lora_received()) {
                payloadSize = lora_receive_packet(dataBuffer, expectedSize);
                lora_receive();
            }
            xSemaphoreGive(radioMutex);

            if (payloadSize == expectedSize) {
                return payloadSize;
            }
        }
        vTaskDelay(LORA_RX_POLL_DELAY);
    }
    return -1;
}

void LoRa_CommunicationClass::processPacket(const LoRa_Packet& receivedHeader) {
    static uint8_t payloadBuffer[LORA_MAX_PACKET_SIZE] = {};

    if (receivedHeader.type == PacketType::ACK) {
        if (ackMutex != nullptr && xSemaphoreTake(ackMutex, portMAX_DELAY) == pdTRUE) {
            receivedAcks.push_back(receivedHeader.messageId);
            xSemaphoreGive(ackMutex);
            ESP_LOGI(TAG_LORA, "Received ACK for msgId=%d", receivedHeader.messageId);
            updateConnectionState(ConnectionState::CONNECTED);
        }
        return;
    }

    if (receivedHeader.type == PacketType::HEADER) {
        sendAckPacketInternal(receivedHeader.messageId);

        ReceivedPacket receivedPacket = {};
        receivedPacket.header = receivedHeader;

        bool validPayload = true;
        if (receivedHeader.payloadLength > 0) {
            int payloadSize = waitForPayload(payloadBuffer, receivedHeader.payloadLength);
            validPayload = payloadSize == receivedHeader.payloadLength;

            if (validPayload) {
                receivedPacket.payload = std::make_unique<uint8_t[]>(receivedHeader.payloadLength);
                std::memcpy(receivedPacket.payload.get(), payloadBuffer, receivedHeader.payloadLength);
                sendAckPacketInternal(receivedHeader.messageId);
            } else {
                ESP_LOGW(TAG_LORA,
                         "Dropping packet msgId=%d due to payload timeout/size mismatch (%d/%d)",
                         receivedHeader.messageId,
                         payloadSize,
                         receivedHeader.payloadLength);
            }
        }

        if (validPayload && xSemaphoreTake(receivedPacketsMutex, portMAX_DELAY) == pdTRUE) {
            receivedPackets.emplace_back(std::move(receivedPacket));
            xSemaphoreGive(receivedPacketsMutex);
        }
        return;
    }

    if (receivedHeader.type == PacketType::PING) {
        LoRa_Packet ackPacket{};
        ackPacket.type = PacketType::ACK;
        ackPacket.messageId = receivedHeader.messageId;
        ackPacket.payloadLength = 0;
        lora_send_packet(reinterpret_cast<uint8_t*>(&ackPacket), sizeof(ackPacket));
    }
}

void LoRa_CommunicationClass::receiveTaskLoop() {
    uint8_t packetBuffer[LORA_MAX_PACKET_SIZE] = {};

    while (true) {
        const bool waitForInterrupt = dio0IsrInstalled;

        if (waitForInterrupt) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            ESP_LOGI(TAG_LORA, "DIO0 interrupt received, checking for packet...");
        }

        const TickType_t radioWait = waitForInterrupt ? portMAX_DELAY : LORA_RX_POLL_DELAY;
        if (radioMutex == nullptr || xSemaphoreTake(radioMutex, radioWait) != pdTRUE) {
            continue;
        }

        int packetSize = 0;
        if (lora_received()) {
            packetSize = lora_receive_packet(packetBuffer, sizeof(packetBuffer));
            lora_receive();
        }

        if (packetSize >= static_cast<int>(sizeof(LoRa_Packet))) {
            LoRa_Packet receivedHeader{};
            std::memcpy(&receivedHeader, packetBuffer, sizeof(receivedHeader));
            processPacket(receivedHeader);
        }

        xSemaphoreGive(radioMutex);
    }
}

