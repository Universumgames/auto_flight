#pragma once

#include <utility>

#include "base.hpp"
#include "RouteData.hpp"

struct PlannedRouteConfirmationPacket: public BasePacket {
    size_t hash;

    std::string toString() override {
        return "PlannedRouteConfirmationPacket{base=" + BasePacket::toString() + ", hash=" + std::to_string(hash) + "}";
    }

    PlannedRouteConfirmationPacket(const PlannedRouteConfirmationPacket& packet) : BasePacket(packet) {
        this->hash = packet.hash;
    }

    PlannedRouteConfirmationPacket(RawSerializedPacket packet, size_t len) : BasePacket() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    PlannedRouteConfirmationPacket(time_t time = 0, size_t hash = 0) : BasePacket(time, PacketType::PLANNED_ROUTE_CONFIRMATION),
                                                                  hash(hash) {}

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(PlannedRouteConfirmationPacket, BasePacket, hash)
};

struct PlannedRoutePacket : public BasePacket {
    std::vector<Coordinate> route;
    RouteSettings settings;
    size_t hash;

    std::string toString() override {
        return "PlannedRoutePacket{base=" + BasePacket::toString() + ", routeSize=" + std::to_string(route.size()) + "}";
    }

    PlannedRoutePacket(const PlannedRoutePacket& packet) : BasePacket(packet) {
        this->hash = packet.hash;
        this->route = std::vector(packet.route);
        this->settings = packet.settings;
    }

    PlannedRoutePacket(RawSerializedPacket packet, size_t len) : BasePacket() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    PlannedRoutePacket(time_t time = 0, std::vector<Coordinate> route = {}, RouteSettings settings = {}) : BasePacket(time, PacketType::PLANNED_ROUTE),
                                                                  route(std::move(route)), settings(settings) {}

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(PlannedRoutePacket, BasePacket, route, settings)
};
