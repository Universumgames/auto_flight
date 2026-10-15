#pragma once
#include "base.hpp"

struct FlightPacket : public BasePacket {
    FlightState state;
    size_t nextWaypointIndex;
    Coordinate nextWaypoint;
    float targetAltitude;

    std::string toString() override {
        return "FlightPacket{base=" + BasePacket::toString() + ", state=" + std::to_string(static_cast<int>(state)) +
               ", nextWaypointIndex=" + std::to_string(nextWaypointIndex) +
               ", nextWaypoint=" + nextWaypoint.toString() +
               ", targetAltitude=" + std::to_string(targetAltitude) + "}";
    }

    FlightPacket(const FlightPacket& packet) : BasePacket(packet) {
        this->state = packet.state;
        this->nextWaypointIndex = packet.nextWaypointIndex;
        this->nextWaypoint = packet.nextWaypoint;
        this->targetAltitude = packet.targetAltitude;
    }

    FlightPacket(RawSerializedPacket packet, size_t len) : BasePacket() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    FlightPacket(time_t time = 0, FlightState state = FlightState::PLANNING, size_t nextWaypointIndex = 0,
                 Coordinate nextWaypoint = Coordinate(),
                 float targetAltitude = 0.0f) : BasePacket(time, PacketType::FLIGHT_DATA), state(state),
                                                nextWaypointIndex(nextWaypointIndex), nextWaypoint(nextWaypoint),
                                                targetAltitude(targetAltitude) {}

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(FlightPacket, BasePacket, state, nextWaypointIndex, nextWaypoint, targetAltitude)
};
