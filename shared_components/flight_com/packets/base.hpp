#pragma once

#include <ctime>

#include "DeviceId.hpp"
#include "esp_log.h"
#include "types.hpp"

using RawSerializedPacket = const uint8_t*;

enum class PacketType: uint8_t {
    // sensors
    SENSOR_UPDATE = 0x10,
    POSITION = 0x11,
    COMPONENT_STATUS = 0x12,

    // flight data
    /// the planned route by the plane to cover a specified area
    PLANNED_ROUTE = 0x30,
    /// the area the plane has to cover
    PLANNED_AREA = 0x31,
    /// the complete history of the plane's route since takeoff
    ROUTE_HISTORY = 0x32,
    /// requesting the complete history of the planes route since takeoff, use with caution
    ROUTE_HISTORY_REQUEST = 0x33,
};

struct IBasePacket {
    virtual std::string toString() = 0;
    virtual ~IBasePacket() = default;
    [[nodiscard]] virtual std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const = 0;
};

struct BasePacket : public IBasePacket {
    time_t timestamp;
    PacketType type;
    uint32_t id;

    std::string toString() override {
        return "BasePacket{timestamp=" + std::to_string(timestamp) + ", type=" + std::to_string(static_cast<int>(type))
            + ", id=" + std::to_string(id) + "}";
    }

    BasePacket(const BasePacket& packet) {
        this->timestamp = packet.timestamp;
        this->type = packet.type;
        this->id = packet.id;
    }

    BasePacket(RawSerializedPacket packet) {
        auto basePacket = (BasePacket*)packet;
        this->timestamp = basePacket->timestamp;
        this->type = basePacket->type;
        this->id = basePacket->id;
    }

    BasePacket(time_t time = 0, PacketType type = PacketType::COMPONENT_STATUS) : timestamp(time), type(type),
        id(DeviceId::get32()) {}

    [[nodiscard]] std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        auto packet = std::make_unique<uint8_t[]>(sizeof(BasePacket));
        std::memcpy(packet.get(), this, sizeof(BasePacket));
        return {std::move(packet), sizeof(BasePacket)};
    }
};
