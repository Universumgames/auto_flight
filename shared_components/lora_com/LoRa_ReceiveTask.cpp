#include "LoRa_Communication.hpp"

#include "esp_log.h"
#include <algorithm>
#include <cstring>
#include <memory>

#include "driver/gpio.h"

#include "DeviceId.hpp"
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

void LoRa_CommunicationClass::sendAckPacketInternal(const LoRa_Packet_Internal& received) {
    LoRa_Packet_Internal ackPacket{};
    ackPacket.type = PacketType::ACK;
    ackPacket.messageId = received.messageId;
    ackPacket.fragmentId = received.fragmentId;
    ackPacket.payloadLength = sizeof(received.senderId);

    // payload: addressee of the ACK
    sendRawPacket(ackPacket, received.senderId, false);
    ackPending = false;
}

void LoRa_CommunicationClass::processPacket(const LoRa_Packet_Internal& receivedHeader, const uint8_t* data,
                                            const int size) {
    ESP_LOGD(TAG_LORA, "Received packet: %s", receivedHeader.toString().c_str());

    if (receivedHeader.type == PacketType::ACK) {
        const auto deviceId = DeviceId::get();
        if (size != sizeof(receivedHeader.senderId) || size != receivedHeader.payloadLength
            || !std::equal(data, data + size, deviceId.begin())) {
            ESP_LOGD(TAG_LORA, "Ignoring ACK for msgId=%u addressed to another device", receivedHeader.messageId);
            return;
        }
        const OutstandingAck ackKey{receivedHeader.messageId, receivedHeader.fragmentId};
        WITH_MUTEX(ackMutex) {
            std::erase(outstandingAcks, ackKey);
        }
    }
    else if (receivedHeader.type == PacketType::PING) {
        sendAckPacketInternal(receivedHeader);
    }
    else {
        ESP_LOGW(TAG_LORA, "Received packet with unsupported type: %d", static_cast<int>(receivedHeader.type));
    }
}

void LoRa_CommunicationClass::processDataPacket(const LoRa_Packet_Internal& header, const uint8_t* data,
                                                const int size) {
    bool validPayload = true;
    validPayload &= size == header.payloadLength;

    if (validPayload) {
        if (header.fragmentId + 1 < header.totalFragments) {
            // more fragments follow: keep our own senders off the channel until the peer is done, including
            // a retransmission if this ACK gets lost
            channelReservedUntil = xTaskGetTickCount()
                + pdMS_TO_TICKS(ackTimeoutMs + maxPacketAirtimeMs + LORA_ACK_MARGIN_MS);
        }
        else {
            channelReservedUntil = xTaskGetTickCount();
        }
        // always (re-)ACK: a duplicate means the sender retransmitted because our previous ACK got lost
        sendAckPacketInternal(header);
        if (isDuplicateFragment(header)) {
            ESP_LOGW(TAG_LORA, "Ignoring retransmitted fragment %u of msgId=%u", header.fragmentId, header.messageId);
            return;
        }

        auto payload = std::make_unique<uint8_t[]>(header.payloadLength);
        std::memcpy(payload.get(), data, header.payloadLength);
        if (header.totalFragments == 1) {
            ReceivedPacket receivedPacket{
                .header = {
                .type = header.type,
                    .senderId = {header.senderId[0], header.senderId[1], header.senderId[2], header.senderId[3]},
                    .messageId = header.messageId,
                    .totalFragments = header.totalFragments,
                    .payloadLength = header.payloadLength,
                },
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
            // DIO1 is edge-triggered: if an edge is ever missed while RX_DONE stays set, DIO1 never drops and
            // no further interrupt arrives. The timeout makes the flag check below recover from that.
            ulTaskNotifyTake(pdTRUE, LORA_RX_IRQ_FALLBACK_POLL);
        }

        int packetSize = 0;
        WITH_MUTEX(radioMutex) {
            // DIO1 also fires on TX_DONE of our own transmissions, so a wakeup doesn't mean a packet arrived.
            // Without RX_DONE the FIFO only holds stale data (TX and RX share it) - leave the radio alone,
            // restarting RX here would abort a packet (e.g. the peer's ACK) that is just coming in.
            if (loraRadio->getIrqFlags() & RADIOLIB_SX126X_IRQ_RX_DONE) {
                size_t pktLen = loraRadio->getPacketLength(true);
                ESP_LOGD(TAG_LORA, "Packet received, length=%zu", pktLen);
                // read at most the buffer size
                size_t toRead = (pktLen > LORA_MAX_PACKET_SIZE) ? LORA_MAX_PACKET_SIZE : pktLen;
                // also clears the IRQ flags; the radio stays in continuous RX mode, no need to restart it
                int16_t res = loraRadio->readData(packetBuffer, toRead);
                if (res == RADIOLIB_ERR_NONE) {
                    packetSize = static_cast<int>(toRead);
                    // claim the radio for our ACK before releasing the mutex, otherwise a waiting local sender
                    // transmits first and the peer gives up on the ACK (or we collide with its retransmission)
                    ackPending = toRead >= sizeof(LoRa_Packet_Internal)
                        && static_cast<PacketType>(packetBuffer[0]) != PacketType::ACK;
                    ESP_LOGD(TAG_LORA, "Packet RSSI=%.1f dBm, SNR=%.1f dB", loraRadio->getRSSI(true),
                             loraRadio->getSNR());
                }
                else {
                    ESP_LOGW(TAG_LORA, "Dropping corrupted packet (err=%d, RSSI=%.1f dBm, SNR=%.1f dB)", res,
                             loraRadio->getRSSI(true), loraRadio->getSNR());
                }
            }
        }

        if (packetSize == 0) {
            // nothing (valid) received
        }
        else if (packetSize >= static_cast<int>(sizeof(LoRa_Packet_Internal))) {
            LoRa_Packet_Internal receivedHeader{};
            std::memcpy(&receivedHeader, packetBuffer, sizeof(LoRa_Packet_Internal));

            if (receivedHeader.type == PacketType::ACK || receivedHeader.type == PacketType::PING) {
                processPacket(receivedHeader, packetBuffer + sizeof(LoRa_Packet_Internal),
                              packetSize - sizeof(LoRa_Packet_Internal));
            }
            else {
                processDataPacket(receivedHeader, packetBuffer + sizeof(LoRa_Packet_Internal),
                                  packetSize - sizeof(LoRa_Packet_Internal));
            }
#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
            const uint32_t senderId = static_cast<uint32_t>(receivedHeader.senderId[0])
                | (static_cast<uint32_t>(receivedHeader.senderId[1]) << 8)
                | (static_cast<uint32_t>(receivedHeader.senderId[2]) << 16)
                | (static_cast<uint32_t>(receivedHeader.senderId[3]) << 24);
            updateConnectionState(senderId, ConnectionState::CONNECTED);
#else
            updateConnectionState(ConnectionState::CONNECTED);
#endif
            updateLastSendTime();
        }
        else {
            ESP_LOGE(TAG_LORA, "Received packet without header or malformed packet");
        }
        // covers packets that were dropped without an ACK (e.g. payload size mismatch)
        ackPending = false;

        vTaskDelay(LORA_RX_POLL_DELAY);
    }
}
