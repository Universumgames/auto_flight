#pragma once
#include <vector>
#include "types.hpp"

namespace Frontend {

struct BaseUpdatePacket {
    static constexpr const char* type = "base";

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BaseUpdatePacket, type)
};

struct FlightUpdatePacket {
    static constexpr const char* type = "flight";
    Coordinate basePosition;
    time_t basePositionUpdateTime;
    Coordinate planePosition;
    time_t planePositionUpdateTime;
    FlightRoute flightRoute;
    time_t flightRouteUpdateTime;
    PlannedRoute plannedRoute;
    time_t plannedRouteUpdateTime;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FlightUpdatePacket, type, basePosition, basePositionUpdateTime, planePosition,
                                   planePositionUpdateTime, flightRoute, flightRouteUpdateTime, plannedRoute,
                                   plannedRouteUpdateTime)
};

struct ConnectionUpdatePacket {
    static constexpr const char* type = "connection";
    ConnectionState baseConnectionState;
    time_t lastContactBaseStationTimestamp;
    ConnectionState planeConnectionState;
    time_t lastContactPlaneTimestamp;

    ConnectionState gpsConnectionBase;
    ConnectionState gpsConnectionPlane;

    ConnectionState barometerConnectionBase;
    ConnectionState barometerConnectionPlane;

    ConnectionState motorComConnectionPlane;
    ConnectionState magnetometerConnectionPlane;
    ConnectionState accelerometerConnectionPlane;
    bool manualOverridePlane;
    FlightState flightState;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionUpdatePacket, type, baseConnectionState, lastContactBaseStationTimestamp,
                                   planeConnectionState, lastContactPlaneTimestamp, gpsConnectionBase,
                                   gpsConnectionPlane, barometerConnectionBase, barometerConnectionPlane,
                                   motorComConnectionPlane, magnetometerConnectionPlane, accelerometerConnectionPlane,
                                   manualOverridePlane, flightState)
};

struct AreaDefinePacket {
    std::vector<Coordinate> shape;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AreaDefinePacket, shape)
};

struct SensorPacket {
    static constexpr const char* type = "sensor";
    float barometerPressureBase;
    float barometerPressurePlane;
    float calculatedAltitude;
    int headingPlane;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SensorPacket, type, barometerPressureBase, barometerPressurePlane, calculatedAltitude, headingPlane)
};

struct PlannedRoutePacket {
    static constexpr const char* type = "plannedRoute";
    std::vector<Coordinate> route;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PlannedRoutePacket, type, route)
};

} // namespace Frontend
