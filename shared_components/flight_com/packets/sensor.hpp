#pragma once

#include "base.hpp"

struct SensorUpdate : public BasePacket {
    // Pressure in hPa
    float pressure;
    // Compass heading in degrees [0, 360), rounded to the nearest int, 0 = magnetic north
    int heading;

    std::string toString() override {
        return "SensorUpdate{timestamp=" + std::to_string(timestamp) + ", type=" +
            std::to_string(static_cast<int>(type)) + ", pressure=" + std::to_string(pressure) +
            ", heading=" + std::to_string(heading) + "}";
    }

    SensorUpdate(const SensorUpdate& packet) : BasePacket(packet) {
        this->pressure = packet.pressure;
        this->heading = packet.heading;
    }

    SensorUpdate(RawSerializedPacket packet) : BasePacket(packet) {
        auto sensorUpdate = (SensorUpdate*)packet;
        this->pressure = sensorUpdate->pressure;
        this->heading = sensorUpdate->heading;
    }

    SensorUpdate(time_t time = 0, float pressure = 0.0, int heading = 0) :
        BasePacket(time, PacketType::SENSOR_UPDATE), pressure(pressure), heading(heading) {}

    std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        auto packet = std::make_unique<uint8_t[]>(sizeof(SensorUpdate));
        std::memcpy(packet.get(), this, sizeof(SensorUpdate));
        return {std::move(packet), sizeof(SensorUpdate)};
    }
};