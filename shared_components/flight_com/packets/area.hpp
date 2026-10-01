#pragma once

#include <utility>

#include "base.hpp"
#include "RouteData.hpp"

struct PlannedAreaPacket : public BasePacket {
    // size_t length; // hidden field in serialized data
    std::vector<Coordinate> shape;
    RouteSettings settings;

    std::string toString() override {
        return "PlannedAreaPacket{base=" + BasePacket::toString() + ", shapeSize=" + std::to_string(shape.size()) + "}";
    }

    PlannedAreaPacket(const PlannedAreaPacket& packet) : BasePacket(packet) {
        this->shape = std::vector(packet.shape);
        this->settings = packet.settings;
    }

    PlannedAreaPacket(RawSerializedPacket packet, size_t len) : BasePacket() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    PlannedAreaPacket(time_t time = 0, std::vector<Coordinate> shape = {}, RouteAlgorithm algorithm = RouteAlgorithm::BASIC, uint8_t overlapPercentage = 20) : BasePacket(time, PacketType::PLANNED_AREA),
                                                                             shape(std::move(shape)),
                                                                             settings(algorithm, overlapPercentage) {}

    PlannedAreaPacket(time_t time, std::vector<Coordinate> shape, RouteSettings settings) : BasePacket(time, PacketType::PLANNED_AREA),
                                                                             shape(std::move(shape)),
                                                                             settings(settings) {}

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(PlannedAreaPacket, BasePacket, shape, settings)
};