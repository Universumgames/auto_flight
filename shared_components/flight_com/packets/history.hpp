#pragma once

#include "base.hpp"

struct FlightHistoryPacket : public BasePacket {
    std::vector<Coordinate> history;

    std::string toString() override {
        return "FlightHistoryPacket{timestamp=" + std::to_string(timestamp) + ", type=" +
            std::to_string(static_cast<int>(type)) + ", historySize=" + std::to_string(history.size()) + "}";
    }

    FlightHistoryPacket(const FlightHistoryPacket& packet) : BasePacket(packet) {
        this->history = std::vector(packet.history);
    }

    FlightHistoryPacket(RawSerializedPacket packet) : BasePacket(packet) {
        size_t dataOffset = sizeof(BasePacket);
        size_t length = 0;
        std::memcpy(&length, packet + dataOffset, sizeof(size_t));
        if (length > 255 || length < 1) {
            ESP_LOGE("FlightHistoryPacket", "Invalid planned area packet with length %u", length);
            return;
        }
        history = std::vector<Coordinate>(length);
        dataOffset += sizeof(size_t);
        memcpy(history.data(), packet + dataOffset, length * sizeof(Coordinate));
    }

    FlightHistoryPacket(time_t time = 0, std::vector<Coordinate> history = {}) : BasePacket(time, PacketType::ROUTE_HISTORY),
                                                                     history(history) {}

    std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        BasePacket packet = {
            timestamp,
            type
        };
        size_t length = history.size();
        size_t dataSize = sizeof(BasePacket) + sizeof(size_t) + (history.size() * sizeof(Coordinate));
        auto data = std::make_unique<uint8_t[]>(dataSize);
        uint8_t dataOffset = 0;
        std::memcpy(data.get(), &packet, sizeof(BasePacket));
        dataOffset += sizeof(BasePacket);
        std::memcpy(data.get() + dataOffset, &length, sizeof(size_t));
        dataOffset += sizeof(size_t);
        std::memcpy(data.get() + dataOffset, history.data(), sizeof(Coordinate) * history.size());
        return {std::move(data), dataSize};
    }
};