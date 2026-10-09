#pragma once

#include "base.hpp"

struct ComponentStatus : public BasePacket {
    ConnectionState gps;
    ConnectionState barometer;
    ConnectionState motorControl;
    ConnectionState magnetometer;
    ConnectionState accelerometer;
    ConnectionState battery;
    bool manualOverride{};
    FlightState flightState;

    std::string toString() override {
        return "ComponentStatus{base=" + BasePacket::toString() +
            ", gps=" + (gps == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", barometer=" + (barometer == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", motorControl=" + (motorControl == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", magnetometer=" + (magnetometer == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", accelerometer=" + (accelerometer == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", battery=" + (battery == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", manualOverride=" + (manualOverride ? "true" : "false") +
            ", flightState=" + std::to_string(static_cast<int>(flightState)) + "}";
    }

    ComponentStatus(const ComponentStatus& packet) : BasePacket(packet) {
        this->gps = packet.gps;
        this->barometer = packet.barometer;
        this->motorControl = packet.motorControl;
        this->magnetometer = packet.magnetometer;
        this->accelerometer = packet.accelerometer;
        this->battery = packet.battery;
        this->manualOverride = packet.manualOverride;
        this->flightState = packet.flightState;
    }

    ComponentStatus(RawSerializedPacket packet, size_t len) : BasePacket(), gps(), barometer(), motorControl(), magnetometer(), accelerometer(), battery(), manualOverride(), flightState() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    ComponentStatus(time_t time = 0, ConnectionState gps = ConnectionState::CONNECTING, ConnectionState barometer = ConnectionState::CONNECTING,
                    ConnectionState motorControl = ConnectionState::CONNECTING, ConnectionState magnetometer = ConnectionState::CONNECTING, ConnectionState accelerometer = ConnectionState::CONNECTING,
                    ConnectionState battery = ConnectionState::CONNECTING, bool manualOverride = false, FlightState flightState = FlightState::PLANNING) :
        BasePacket(time, PacketType::COMPONENT_STATUS), gps(gps), barometer(barometer),
        motorControl(motorControl), magnetometer(magnetometer), accelerometer(accelerometer), battery(battery), manualOverride(manualOverride),
        flightState(flightState) {}

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(ComponentStatus, BasePacket, gps, barometer, motorControl, magnetometer, accelerometer, battery, manualOverride, flightState)

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }
};
