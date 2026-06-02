#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include <algorithm>
#include <cstring>
#include <memory>

#include "driver/gpio.h"

#include "FlightStorage.hpp"
#include "helper.hpp"
#include "mutex_helper.hpp"
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
    WITH_MUTEX_CUSTOM_DELAY(radioMutex, LORA_RX_POLL_DELAY) {
        LoRa_Packet ackPacket{};
        ackPacket.type = PacketType::ACK;
        ackPacket.messageId = messageId;
        ackPacket.payloadLength = 0;
        // transmit the ACK and return the radio to receive mode
        loraRadio->transmit(reinterpret_cast<uint8_t*>(&ackPacket), sizeof(ackPacket));
        loraRadio->finishTransmit();
        loraRadio->startReceive();
    }
    updateLastSendTime();
}

int LoRa_CommunicationClass::waitForPayload(uint8_t* dataBuffer, uint8_t expectedSize) {
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < LORA_PAYLOAD_TIMEOUT) {
        WITH_MUTEX(radioMutex) {
            int payloadSize = 0;
            // ask radio for packet length (update cached value)
            size_t pktLen = loraRadio->getPacketLength(true);
            if (pktLen > 0) {
                // only read up to expectedSize to avoid buffer overflow
                size_t toRead = (pktLen > expectedSize) ? expectedSize : pktLen;
                int16_t res = loraRadio->readData(dataBuffer, toRead);
                // return to RX mode after reading
                loraRadio->startReceive();
                if (res == RADIOLIB_ERR_NONE || res == RADIOLIB_ERR_CRC_MISMATCH || res ==
                    RADIOLIB_ERR_LORA_HEADER_DAMAGED) {
                    payloadSize = static_cast<int>(toRead);
                }
                else {
                    payloadSize = 0;
                }
            }
            if (payloadSize == expectedSize) {
                // manually releasing semaphore to work with return
                xSemaphoreGive(radioMutex);
                return payloadSize;
            }
        }

        vTaskDelay(LORA_RX_POLL_DELAY);
    }
    return -1;
}

void LoRa_CommunicationClass::processPacket(const LoRa_Packet& receivedHeader) {
    static uint8_t payloadBuffer[LORA_MAX_PACKET_SIZE] = {};

    ESP_LOGI(TAG_LORA, "Received (header) packet: %s", receivedHeader.toString().c_str());

    if (receivedHeader.type == PacketType::ACK) {
        WITH_MUTEX(receivedPacketsMutex) {
            receivedAcks.push_back(receivedHeader.messageId);
        }
        ESP_LOGI(TAG_LORA, "Received ACK for msgId=%d", receivedHeader.messageId);
        updateConnectionState(ConnectionState::CONNECTED);
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
            }
            else {
                ESP_LOGW(TAG_LORA,
                         "Dropping packet msgId=%d due to payload timeout/size mismatch (%d/%d)",
                         receivedHeader.messageId,
                         payloadSize,
                         receivedHeader.payloadLength);
            }
        }

        if (validPayload) {
            WITH_MUTEX(receivedPacketsMutex) {
                receivedPackets.emplace_back(std::move(receivedPacket));
            }
        }
        return;
    }

    if (receivedHeader.type == PacketType::PING) {
        ESP_LOGD(TAG_LORA, "Received PING packet, sending ACK");
        LoRa_Packet ackPacket{};
        ackPacket.type = PacketType::ACK;
        ackPacket.messageId = receivedHeader.messageId;
        ackPacket.payloadLength = 0;
        WITH_MUTEX(radioMutex) {
            loraRadio->transmit(reinterpret_cast<uint8_t*>(&ackPacket), sizeof(ackPacket));
            loraRadio->startReceive();
        }
    }

    updateLastSendTime();
}

void LoRa_CommunicationClass::receiveTaskLoop() {
    uint8_t packetBuffer[LORA_MAX_PACKET_SIZE] = {};

    while (true) {
        if (dio0IsrInstalled) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            ESP_LOGD(TAG_LORA, "DIO0 interrupt received, checking for packet...");
        }

        int packetSize = 0;
        WITH_MUTEX(radioMutex) {
            size_t pktLen = loraRadio->getPacketLength(true);
            ESP_LOGD(TAG_LORA, "Checking for packet, length=%zu", pktLen);
            if (pktLen > 0) {
                // read at most the buffer size
                size_t toRead = (pktLen > sizeof(packetBuffer)) ? sizeof(packetBuffer) : pktLen;
                int16_t res = loraRadio->readData(packetBuffer, toRead);
                // return to receive mode
                loraRadio->startReceive();
                if (res == RADIOLIB_ERR_NONE || res == RADIOLIB_ERR_CRC_MISMATCH || res ==
                    RADIOLIB_ERR_LORA_HEADER_DAMAGED) {
                    packetSize = static_cast<int>(toRead);
                }
                else {
                    packetSize = 0;
                }
            }
        }

        if (packetSize >= static_cast<int>(sizeof(LoRa_Packet))) {
            LoRa_Packet receivedHeader{};
            std::memcpy(&receivedHeader, packetBuffer, sizeof(receivedHeader));
            processPacket(receivedHeader);
        }
        vTaskDelay(1);
    }
}
