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

void LoRa_CommunicationClass::receiveTaskEntry(void* param) {
    auto* instance = static_cast<LoRa_CommunicationClass*>(param);
    instance->receiveTaskLoop();
}

void LoRa_CommunicationClass::sendAckPacketInternal(uint8_t messageId) {
    LoRa_Packet_Internal ackPacket{};
    ackPacket.type = PacketType::ACK;
    ackPacket.messageId = messageId;
    ackPacket.payloadLength = 0;

    sendRawPacket(ackPacket, nullptr, false);
}

void LoRa_CommunicationClass::processPacket(const LoRa_Packet_Internal& receivedHeader) {
    ESP_LOGD(TAG_LORA, "Received packet: %s", receivedHeader.toString().c_str());

    if (receivedHeader.type == PacketType::ACK) {
        WITH_MUTEX(receivedPacketsMutex) {
            erase_if(outstandingAcks, [&receivedHeader](const uint8_t& id) { return id == receivedHeader.messageId; });
        }
    }
    else if (receivedHeader.type == PacketType::PING) {
        sendAckPacketInternal(receivedHeader.messageId);
    }
    else {
        ESP_LOGW(TAG_LORA, "Received packet with unsupported type: %d", static_cast<int>(receivedHeader.type));
    }
    nextMessageId.exchange(receivedHeader.messageId + 1);
}

void LoRa_CommunicationClass::processDataPacket(const LoRa_Packet_Internal& header, const uint8_t* data,
                                                const int size) {
    bool validPayload = true;
    validPayload &= size == header.payloadLength;

    if (validPayload) {
        sendAckPacketInternal(header.messageId);

        auto payload = std::make_unique<uint8_t[]>(header.payloadLength);
        std::memcpy(payload.get(), data, header.payloadLength);
        if (header.totalFragments == 1) {
            ReceivedPacket receivedPacket{
                .header = header,
                .payload = std::move(payload),
            };
            WITH_MUTEX(receivedPacketsMutex) {
                receivedPackets.emplace_back(std::move(receivedPacket));
            }
        }
        else {
            PacketFragment fragment{
                .messageId = header.messageId,
                .fragmentId = header.fragmentId,
                .payloadLength = header.payloadLength,
                .payload = std::move(payload),
            };
            ESP_LOGI(TAG_LORA, "Received fragment %d/%d for messageId=%d with payload size %d",
                 header.fragmentId + 1, header.totalFragments, header.messageId,
                 header.payloadLength);
            WITH_MUTEX(receivedFragmentsMutex) {
                if (!receivedFragments.contains(header.messageId)) {
                    receivedFragments[header.messageId] = ReceivedFragmentsCache{
                        .header = header,
                        .fragments = {}
                    };
                }
                if (std::ranges::contains(receivedFragments[header.messageId].fragments, fragment)) {
                    ESP_LOGE(TAG_LORA, "Received fragment %u for message %u multiple times", header.fragmentId,
                             header.messageId);
                }
                else
                    receivedFragments[header.messageId].fragments.emplace_back(std::move(fragment));
            }
        }
    }
    else {
        ESP_LOGW(TAG_LORA,
                 "Dropping packet msgId=%d due to payload timeout/size mismatch (%d/%d)",
                 header.messageId,
                 size,
                 header.payloadLength);
    }
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
                size_t toRead = (pktLen > LORA_MAX_PACKET_SIZE) ? LORA_MAX_PACKET_SIZE : pktLen;
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

        if (packetSize >= static_cast<int>(sizeof(LoRa_Packet_Internal))) {
            LoRa_Packet_Internal receivedHeader{};
            std::memcpy(&receivedHeader, packetBuffer, sizeof(LoRa_Packet_Internal));

            if (isOwnPacket(receivedHeader)) {
                ESP_LOGD(TAG_LORA, "Received own packet (type=%d, msgId=%d), ignoring", receivedHeader.type,
                         receivedHeader.messageId);
                continue;
            }

            if (receivedHeader.type == PacketType::ACK || receivedHeader.type == PacketType::PING) {
                processPacket(receivedHeader);
            }
            else {
                processDataPacket(receivedHeader, packetBuffer + sizeof(LoRa_Packet_Internal),
                                  packetSize - sizeof(LoRa_Packet_Internal));
            }
            updateConnectionState(ConnectionState::CONNECTED);
            updateLastSendTime();
        }
        else {
            ESP_LOGE(TAG_LORA, "Received packet without header or malformed packet");
        }

        vTaskDelay(LORA_RX_POLL_DELAY);
    }
}
