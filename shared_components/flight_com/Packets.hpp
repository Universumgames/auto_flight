#pragma once
#include <ctime>

#include "types.hpp"

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

struct BasePacket {
    time_t timestamp;
    PacketType type;
};

struct SensorUpdate : public BasePacket {
    // Pressure in hPa
    float pressure;
};

struct PositionUpdate : public BasePacket {
    // GPS position
    Coordinate position;
};

struct PlannedAreaPacket : public BasePacket {
    // size_t length; // hidden field in serialized data
    std::vector<Coordinate> shape;
};

struct FlightHistoryPacket : public BasePacket {
    std::vector<Coordinate> history;
};

struct PlannedRoutePacket : public BasePacket {
    std::vector<Coordinate> route;
};

struct ComponentStatus: public BasePacket {
    ConnectionState gps;
    ConnectionState barometer;
    ConnectionState gyroscope;
    ConnectionState motorControl;
};