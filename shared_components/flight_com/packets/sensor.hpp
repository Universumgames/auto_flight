#pragma once

#include "base.hpp"

struct SensorUpdate : public BasePacket {
    // Pressure in hPa
    float pressure;

    std::string toString() override {
        return "SensorUpdate{timestamp=" + std::to_string(timestamp) + ", type=" +
            std::to_string(static_cast<int>(type)) + ", pressure=" + std::to_string(pressure) + "}";
    }

    SensorUpdate(const SensorUpdate& packet) : BasePacket(packet) {
        this->pressure = packet.pressure;
    }

    SensorUpdate(RawSerializedPacket packet) : BasePacket(packet) {
        auto sensorUpdate = (SensorUpdate*)packet;
        this->pressure = sensorUpdate->pressure;
    }

    SensorUpdate(time_t time = 0, float pressure = 0.0) : BasePacket(time, PacketType::SENSOR_UPDATE), pressure(pressure) {}

    std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        auto packet = std::make_unique<uint8_t[]>(sizeof(SensorUpdate));
        std::memcpy(packet.get(), this, sizeof(SensorUpdate));
        return {std::move(packet), sizeof(SensorUpdate)};
    }
};