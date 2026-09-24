#pragma once

#include <utility>

#include "base.hpp"

struct FlightHistoryPacket : public BasePacket {
    std::vector<Coordinate> history;

    std::string toString() override {
        return "FlightHistoryPacket{base=" + BasePacket::toString() + ", historySize=" + std::to_string(history.size()) + "}";
    }

    FlightHistoryPacket(const FlightHistoryPacket& packet) : BasePacket(packet) {
        this->history = std::vector(packet.history);
    }


    FlightHistoryPacket(RawSerializedPacket packet, size_t len) : BasePacket() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    FlightHistoryPacket(time_t time = 0, std::vector<Coordinate> history = {}) : BasePacket(time, PacketType::ROUTE_HISTORY),
                                                                     history(std::move(history)) {}

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(FlightHistoryPacket, BasePacket, history)
};