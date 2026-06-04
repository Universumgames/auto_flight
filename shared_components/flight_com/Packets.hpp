#pragma once
#include <ctime>

#include "types.hpp"

using RawSerializedPacket = const uint8_t*;

enum class PacketType: uint8_t {
    // sensors
    SENSOR_UPDATE = 0x10,
    POSITION = 0x11,
    COMPONENT_STATUS = 0x12,

    // flight data
    /// the planned route by the plane to cover a specified area
    PLANNED_ROUTE = 0x30,
    /// the area the plane has to cover
    PLANNED_AREA = 0x31,
    /// the complete history of the plane's route since takeoff
    ROUTE_HISTORY = 0x32,
    /// requesting the complete history of the planes route since takeoff, use with caution
    ROUTE_HISTORY_REQUEST = 0x33,
};

struct IBasePacket {
    virtual std::string toString() = 0;
    virtual ~IBasePacket() = default;
};

struct BasePacket : public IBasePacket {
    time_t timestamp;
    PacketType type;

    std::string toString() override {
        return "BasePacket{timestamp=" + std::to_string(timestamp) + ", type=" + std::to_string(static_cast<int>(type))
            + "}";
    }

    BasePacket(const BasePacket& packet) {
        this->timestamp = packet.timestamp;
        this->type = packet.type;
    }

    BasePacket(RawSerializedPacket packet) {
        auto basePacket = (BasePacket*)packet;
        this->timestamp = basePacket->timestamp;
        this->type = basePacket->type;
    }

    BasePacket(time_t time = 0, PacketType type = PacketType::COMPONENT_STATUS) : timestamp(time), type(type) {}
};

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
};

struct PositionUpdate : public BasePacket {
    // GPS position
    Coordinate position;

    std::string toString() override {
        return "PositionUpdate{timestamp=" + std::to_string(timestamp) + ", type=" +
            std::to_string(static_cast<int>(type)) + ", position=" + position.toString() + "}";
    }

    PositionUpdate(const PositionUpdate& packet) : BasePacket(packet) {
        this->position = packet.position;
    }

    PositionUpdate(RawSerializedPacket packet) : BasePacket(packet) {
        auto positionUpdate = (PositionUpdate*)packet;
        this->position = positionUpdate->position;
    }

    PositionUpdate(time_t time = 0, Coordinate position = COORDINATE_INIT_INVALID()) : BasePacket(time, PacketType::POSITION), position(position) {}
};

struct PlannedAreaPacket : public BasePacket {
    // size_t length; // hidden field in serialized data
    std::vector<Coordinate> shape;

    std::string toString() override {
        return "PlannedAreaPacket{timestamp=" + std::to_string(timestamp) + ", type=" +
            std::to_string(static_cast<int>(type)) + ", shapeSize=" + std::to_string(shape.size()) + "}";
    }

    PlannedAreaPacket(const PlannedAreaPacket& packet) : BasePacket(packet) {
        this->shape = std::vector(packet.shape);
    }

    PlannedAreaPacket(RawSerializedPacket packet) : BasePacket(packet) {
        auto basePacket = (BasePacket*)packet;
        size_t length = 0;
        std::memcpy(&length, basePacket + sizeof(BasePacket), sizeof(size_t));
        this->shape.resize(length);
        std::memcpy(this->shape.data(), basePacket + sizeof(BasePacket) + sizeof(size_t), length * sizeof(Coordinate));
    }

    PlannedAreaPacket(time_t time = 0, std::vector<Coordinate> shape = {}) : BasePacket(time, PacketType::PLANNED_AREA),
                                                                 shape(shape) {}
};

struct FlightHistoryPacket : public BasePacket {
    std::vector<Coordinate> history;

    std::string toString() override {
        return "FlightHistoryPacket{timestamp=" + std::to_string(timestamp) + ", type=" +
            std::to_string(static_cast<int>(type)) + ", historySize=" + std::to_string(history.size()) + "}";
    }

    FlightHistoryPacket(const FlightHistoryPacket& packet) : BasePacket(packet) {
        this->history = std::vector(packet.history);
    }

    FlightHistoryPacket(RawSerializedPacket packet) : BasePacket(packet) {
        auto historyPacket = (FlightHistoryPacket*)packet;
        size_t length = 0;
        std::memcpy(&length, historyPacket + sizeof(BasePacket), sizeof(size_t));
        this->history.resize(length);
        std::memcpy(this->history.data(), historyPacket + sizeof(size_t), length * sizeof(Coordinate));
    }

    FlightHistoryPacket(time_t time = 0, std::vector<Coordinate> history = {}) : BasePacket(time, PacketType::ROUTE_HISTORY),
                                                                     history(history) {}
};

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
        auto routePacket = (PlannedRoutePacket*)packet;
        size_t length = 0;
        std::memcpy(&length, routePacket + sizeof(BasePacket), sizeof(size_t));
        this->route.resize(length);
        std::memcpy(this->route.data(), routePacket + sizeof(size_t), length * sizeof(Coordinate));
    }

    PlannedRoutePacket(time_t time = 0, std::vector<Coordinate> route = {}) : BasePacket(time, PacketType::PLANNED_ROUTE),
                                                                  route(route) {}
};

struct ComponentStatus : public BasePacket {
    ConnectionState gps;
    ConnectionState barometer;
    ConnectionState gyroscope;
    ConnectionState motorControl;

    std::string toString() override {
        return "ComponentStatus{timestamp=" + std::to_string(timestamp) + ", type=" + std::to_string(
                static_cast<int>(type)) +
            ", gps=" + (gps == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", barometer=" + (barometer == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", gyroscope=" + (gyroscope == ConnectionState::CONNECTED ? "connected" : "connecting") +
            ", motorControl=" + (motorControl == ConnectionState::CONNECTED ? "connected" : "connecting") + "}";
    }

    ComponentStatus(const ComponentStatus& packet) : BasePacket(packet) {
        this->gps = packet.gps;
        this->barometer = packet.barometer;
        this->gyroscope = packet.gyroscope;
        this->motorControl = packet.motorControl;
    }

    ComponentStatus(RawSerializedPacket packet) : BasePacket(packet) {
        auto gpsPacket = (ComponentStatus*)packet;
        this->gps = gpsPacket->gps;
        this->barometer = gpsPacket->barometer;
        this->gyroscope = gpsPacket->gyroscope;
        this->motorControl = gpsPacket->motorControl;
    }

    ComponentStatus(time_t time = 0, ConnectionState gps = ConnectionState::CONNECTING, ConnectionState barometer = ConnectionState::CONNECTING, ConnectionState gyroscope = ConnectionState::CONNECTING,
                    ConnectionState motorControl = ConnectionState::CONNECTING) :
        BasePacket(time, PacketType::COMPONENT_STATUS), gps(gps), barometer(barometer), gyroscope(gyroscope),
        motorControl(motorControl) {}
};
