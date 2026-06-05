#pragma once
#include "base.hpp"

struct PositionUpdate : public BasePacket {
    // GPS position
    Coordinate position;

    std::string toString() override {
        return "PositionUpdate{timestamp=" + std::to_string(timestamp) + ", type=" +
            std::to_string(static_cast<int>(type)) + ", position=" + position.toString() + "}";
    }

    PositionUpdate(const PositionUpdate& packet) : BasePacket(packet) {
        this->position = packet.position;
    }

    PositionUpdate(RawSerializedPacket packet) : BasePacket(packet) {
        auto positionUpdate = (PositionUpdate*)packet;
        this->position = positionUpdate->position;
    }

    PositionUpdate(time_t time = 0, Coordinate position = COORDINATE_INIT_INVALID()) : BasePacket(time, PacketType::POSITION), position(position) {}

    std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        auto packet = std::make_unique<uint8_t[]>(sizeof(PositionUpdate));
        std::memcpy(packet.get(), this, sizeof(PositionUpdate));
        return {std::move(packet), sizeof(PositionUpdate)};
    }
};