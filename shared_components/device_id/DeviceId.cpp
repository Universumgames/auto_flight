#include "DeviceId.hpp"

#include "esp_mac.h"

namespace {
    /**
     * Fowler Noll–Vo hash function, version 1a, 32-bit. Used to hash the device's MAC address into a 32-bit identifier.
     * @param data data to hash
     * @param len length of data to hash
     * @return 32-bit hash of the input data
     */
    uint32_t fnv1aHash(const uint8_t* data, const size_t len) {
        uint32_t hash = 0x811c9dc5;
        for (size_t i = 0; i < len; i++) {
            hash ^= data[i];
            hash *= 0x01000193;
        }
        return hash;
    }
}

uint32_t DeviceId::get32() {
    static uint32_t id = {};
    static bool initialized = false;
    if (!initialized) {
        uint8_t mac[6] = {};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        const uint32_t hash = fnv1aHash(mac, sizeof(mac));
        id = hash;
        initialized = true;
    }
    return id;
}

std::array<uint8_t, 4> DeviceId::get() {
    static std::array<uint8_t, 4> id{};
    static bool initialized = false;
    if (!initialized) {
        const uint32_t hash = get32();
        id = {
            static_cast<uint8_t>(hash),
            static_cast<uint8_t>(hash >> 8),
            static_cast<uint8_t>(hash >> 16),
            static_cast<uint8_t>(hash >> 24)
        };
        initialized = true;
    }
    return id;
}
