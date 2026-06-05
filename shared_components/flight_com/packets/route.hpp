#pragma once

#include "base.hpp"

struct PlannedRoutePacket : public BasePacket {
    std::vector<Coordinate> route;

    std::string toString() override {
        return "PlannedRoutePacket{timestamp=" + std::to_string(timestamp) + ", type=" +
            std::to_string(static_cast<int>(type)) + ", routeSize=" + std::to_string(route.size()) + "}";
    }

    PlannedRoutePacket(const PlannedRoutePacket& packet) : BasePacket(packet) {
        this->route = std::vector(packet.route);
    }

    PlannedRoutePacket(RawSerializedPacket packet) : BasePacket(packet) {
        size_t dataOffset = sizeof(BasePacket);
        size_t length = 0;
        std::memcpy(&length, packet + dataOffset, sizeof(size_t));
        if (length > 255 || length < 1) {
            ESP_LOGE("PlannedRoutePacket", "Invalid planned area packet with length %u", length);
            return;
        }
        route = std::vector<Coordinate>(length);
        dataOffset += sizeof(size_t);
        memcpy(route.data(), packet + dataOffset, length * sizeof(Coordinate));
    }

    PlannedRoutePacket(time_t time = 0, std::vector<Coordinate> route = {}) : BasePacket(time, PacketType::PLANNED_ROUTE),
                                                                  route(route) {}

    std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        BasePacket packet = {
            timestamp,
            type
        };
        size_t length = route.size();
        size_t dataSize = sizeof(BasePacket) + sizeof(size_t) + (route.size() * sizeof(Coordinate));
        auto data = std::make_unique<uint8_t[]>(dataSize);
        uint8_t dataOffset = 0;
        std::memcpy(data.get(), &packet, sizeof(BasePacket));
        dataOffset += sizeof(BasePacket);
        std::memcpy(data.get() + dataOffset, &length, sizeof(size_t));
        dataOffset += sizeof(size_t);
        std::memcpy(data.get() + dataOffset, route.data(), sizeof(Coordinate) * route.size());
        return {std::move(data), dataSize};
    }
};