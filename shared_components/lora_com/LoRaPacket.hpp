#pragma once
#include <cstddef>
#include <cstdint>

// Plain wire-format struct, split out of LoRa_Communication.hpp so consumers
// that only need the packet shape (e.g. Flight_Communication's packet codec)
//
struct LoRaPacket {
    size_t length;
    uint8_t* payload;
};
