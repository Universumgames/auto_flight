#pragma once
#include <array>
#include <cstdint>

namespace DeviceId {
    /**
     * Returns this device's 4-byte identifier, derived once from the ESP32's factory MAC
     * (hashed, not truncated, so devices from the same batch/OUI don't correlate) and
     * cached for the lifetime of the process.
     */
    std::array<uint8_t, 4> get();

    /**
     * Returns this device's 32-bit identifier, derived once from the ESP32's factory MAC
     * (hashed, not truncated, so devices from the same batch/OUI don't correlate) and
     * cached for the lifetime of the process.
     */
    uint32_t get32();
}
