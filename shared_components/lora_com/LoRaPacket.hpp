#pragma once
#include <cstddef>
#include <cstdint>

// Plain wire-format struct, split out of LoRa_Communication.hpp so consumers
// that only need the packet shape (e.g. Flight_Communication's packet codec)
// don't have to pull in the FreeRTOS/radiolib radio driver to get it.
struct LoRaPacket {
    size_t length;
    uint8_t* payload;
};
