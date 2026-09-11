#include "DeviceId.hpp"

std::array<uint8_t, 4> DeviceId::get() {
    static std::array<uint8_t, 4> id{};
    return id;
}