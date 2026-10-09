#include "esp_log.h"
#include "LoRa_Communication.hpp"
#include "mutex_helper.hpp"
#include "freertos/task.h"
#include <ranges>

void LoRa_CommunicationClass::joinFragmentsEntry(void* param) {
    getInstancePtr()->joinFragmentsLoop();
}

void LoRa_CommunicationClass::joinFragmentsLoop() {
    while (true) {
        WITH_MUTEX(receivedFragmentsMutex) {
            std::vector<uint8_t> fragmentsToRemove;
            for (auto& fragments_cache : receivedFragments | std::ranges::views::values) {
                if (fragments_cache.header.totalFragments == fragments_cache.fragments.size()) {
                    try {
                        auto data = joinData(fragments_cache);
                        WITH_MUTEX(receivedPacketsMutex) {
                            receivedPackets.emplace_back(std::move(data));
                        }
                        fragmentsToRemove.emplace_back(fragments_cache.header.messageId);
                    }catch (std::exception& e) {
                        ESP_LOGE("LoRaJoinFragments", "%s", e.what());
                    }
                }
            }
            for (auto mid : fragmentsToRemove) {
                receivedFragments.erase(mid);
            }
        }

        vTaskDelay(LORA_RX_POLL_DELAY);
    }
}
