#include "LoRa_Communication.hpp"
#include "mutex_helper.hpp"
#include "freertos/FreeRTOS.h"

void LoRa_CommunicationClass::callbackWorkerEntry(void* param) {
    auto* instance = static_cast<LoRa_CommunicationClass*>(param);
    instance->callbackWorkerLoop();
}

[[noreturn]] void LoRa_CommunicationClass::callbackWorkerLoop() {
    while (true) {
        // Process received packets and invoke callbacks
        std::vector<ReceivedPacket> packetsToProcess;

        WITH_MUTEX(receivedPacketsMutex) {
            packetsToProcess = std::move(receivedPackets);
            receivedPackets.clear();
        }

        for (const auto& packet : packetsToProcess) {
            for (const auto& callback : receivePacketCallbacks) {
                callback({packet.header.payloadLength, packet.payload.get()});
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Adjust delay as needed
    }
}
