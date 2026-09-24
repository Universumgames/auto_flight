#pragma once

#include "base.hpp"

struct SensorUpdate : public BasePacket {
    // Pressure in hPa
    float pressure;
    // Compass heading in degrees [0, 360), rounded to the nearest int, 0 = magnetic north
    int heading;
    // Battery percentage (0-100)
    int batteryPercent;

    std::string toString() override {
        return "SensorUpdate{base=" + BasePacket::toString() + ", pressure=" + std::to_string(pressure) +
            ", heading=" + std::to_string(heading) + ", batteryPercent=" + std::to_string(batteryPercent) + "}";
    }

    SensorUpdate(const SensorUpdate& packet) : BasePacket(packet) {
        this->pressure = packet.pressure;
        this->heading = packet.heading;
        this->batteryPercent = packet.batteryPercent;
    }

    SensorUpdate(RawSerializedPacket packet, size_t len) : BasePacket() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    SensorUpdate(time_t time = 0, float pressure = 0.0, int heading = 0, int batteryPercent = 0) :
        BasePacket(time, PacketType::SENSOR_UPDATE), pressure(pressure), heading(heading), batteryPercent(batteryPercent) {}

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(SensorUpdate, BasePacket, pressure, heading, batteryPercent)
};