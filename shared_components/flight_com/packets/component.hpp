#pragma once

#include "base.hpp"

struct ComponentStatus : public BasePacket {
    ConnectionState gps;
    ConnectionState barometer;
    ConnectionState gyroscope;
    ConnectionState motorControl;
    ConnectionState magnetometer;

    std::string toString() override {
        return "ComponentStatus{timestamp=" + std::to_string(timestamp) + ", type=" + std::to_string(
                static_cast<int>(type)) +
            ", gps=" + (gps == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", barometer=" + (barometer == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", gyroscope=" + (gyroscope == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", motorControl=" + (motorControl == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", magnetometer=" + (magnetometer == ConnectionState::CONNECTED ? "connected" : "connecting") + "}";
    }

    ComponentStatus(const ComponentStatus& packet) : BasePacket(packet) {
        this->gps = packet.gps;
        this->barometer = packet.barometer;
        this->gyroscope = packet.gyroscope;
        this->motorControl = packet.motorControl;
        this->magnetometer = packet.magnetometer;
    }

    ComponentStatus(RawSerializedPacket packet) : BasePacket(packet) {
        auto gpsPacket = (ComponentStatus*)packet;
        this->gps = gpsPacket->gps;
        this->barometer = gpsPacket->barometer;
        this->gyroscope = gpsPacket->gyroscope;
        this->motorControl = gpsPacket->motorControl;
        this->magnetometer = gpsPacket->magnetometer;
    }

    ComponentStatus(time_t time = 0, ConnectionState gps = ConnectionState::CONNECTING, ConnectionState barometer = ConnectionState::CONNECTING, ConnectionState gyroscope = ConnectionState::CONNECTING,
                    ConnectionState motorControl = ConnectionState::CONNECTING, ConnectionState magnetometer = ConnectionState::CONNECTING) :
        BasePacket(time, PacketType::COMPONENT_STATUS), gps(gps), barometer(barometer), gyroscope(gyroscope),
        motorControl(motorControl), magnetometer(magnetometer) {}

    std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        auto packet = std::make_unique<uint8_t[]>(sizeof(ComponentStatus));
        std::memcpy(packet.get(), this, sizeof(ComponentStatus));
        return {std::move(packet), sizeof(ComponentStatus)};
    }
};
