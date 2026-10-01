#include "DeviceId.hpp"

std::array<uint8_t, 4> DeviceId::get() {
    static std::array<uint8_t, 4> id{};
    return id;
}

uint32_t DeviceId::get32() {
    static uint32_t id = {};
    return id;
}
uint32_t DeviceId::getBaseStationId() {
    return 0;
}
