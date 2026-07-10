#pragma once

#include "base.hpp"

struct ComponentStatus : public BasePacket {
    ConnectionState gps;
    ConnectionState barometer;
    ConnectionState motorControl;
    ConnectionState magnetometer;
    ConnectionState accelerometer;
    bool manualOverride;
    FlightState flightState;

    std::string toString() override {
        return "ComponentStatus{timestamp=" + std::to_string(timestamp) + ", type=" + std::to_string(
                static_cast<int>(type)) +
            ", gps=" + (gps == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", barometer=" + (barometer == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", motorControl=" + (motorControl == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", magnetometer=" + (magnetometer == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", accelerometer=" + (accelerometer == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", manualOverride=" + (manualOverride ? "true" : "false") +
            ", flightState=" + std::to_string(static_cast<int>(flightState)) + "}";
    }

    ComponentStatus(const ComponentStatus& packet) : BasePacket(packet) {
        this->gps = packet.gps;
        this->barometer = packet.barometer;
        this->motorControl = packet.motorControl;
        this->magnetometer = packet.magnetometer;
        this->accelerometer = packet.accelerometer;
        this->manualOverride = packet.manualOverride;
        this->flightState = packet.flightState;
    }

    ComponentStatus(RawSerializedPacket packet) : BasePacket(packet) {
        auto gpsPacket = (ComponentStatus*)packet;
        this->gps = gpsPacket->gps;
        this->barometer = gpsPacket->barometer;
        this->motorControl = gpsPacket->motorControl;
        this->magnetometer = gpsPacket->magnetometer;
        this->accelerometer = gpsPacket->accelerometer;
        this->manualOverride = gpsPacket->manualOverride;
        this->flightState = gpsPacket->flightState;
    }

    ComponentStatus(time_t time = 0, ConnectionState gps = ConnectionState::CONNECTING, ConnectionState barometer = ConnectionState::CONNECTING,
                    ConnectionState motorControl = ConnectionState::CONNECTING, ConnectionState magnetometer = ConnectionState::CONNECTING, ConnectionState accelerometer = ConnectionState::CONNECTING,
                    bool manualOverride = false, FlightState flightState = FlightState::PLANNING) :
        BasePacket(time, PacketType::COMPONENT_STATUS), gps(gps), barometer(barometer),
        motorControl(motorControl), magnetometer(magnetometer), accelerometer(accelerometer), manualOverride(manualOverride),
        flightState(flightState) {}

    std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        auto packet = std::make_unique<uint8_t[]>(sizeof(ComponentStatus));
        std::memcpy(packet.get(), this, sizeof(ComponentStatus));
        return {std::move(packet), sizeof(ComponentStatus)};
    }
};
