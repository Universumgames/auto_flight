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

    PositionUpdate(RawSerializedPacket packet, size_t len) : BasePacket() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    PositionUpdate(time_t time = 0, Coordinate position = COORDINATE_INIT_INVALID()) : BasePacket(time, PacketType::POSITION), position(position) {}

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(PositionUpdate, BasePacket, position)
};