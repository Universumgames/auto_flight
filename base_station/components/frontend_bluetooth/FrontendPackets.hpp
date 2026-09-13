#pragma once
#include <vector>
#include "types.hpp"

namespace Frontend {

/// Common base for packets exchanged with the frontend that concern a specific plane. Carries
/// the plane's device id as real, explicit protocol data, so neither side has to guess which
/// plane an update is about (mirrors BasePacket::id in flight_com).
struct BaseUpdatePacket {
    static constexpr const char* type = "base";
    uint32_t planeId = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BaseUpdatePacket, type, planeId)
};

struct FlightUpdatePacket : public BaseUpdatePacket {
    static constexpr const char* type = "flight";
    Coordinate basePosition;
    time_t basePositionUpdateTime;
    Coordinate planePosition;
    time_t planePositionUpdateTime;
    FlightRoute flightRoute;
    time_t flightRouteUpdateTime;
    PlannedRoute plannedRoute;
    time_t plannedRouteUpdateTime;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FlightUpdatePacket, type, planeId, basePosition, basePositionUpdateTime,
                                   planePosition, planePositionUpdateTime, flightRoute, flightRouteUpdateTime,
                                   plannedRoute, plannedRouteUpdateTime)
};

struct ConnectionUpdatePacket : public BaseUpdatePacket {
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

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionUpdatePacket, type, planeId, baseConnectionState,
                                   lastContactBaseStationTimestamp, planeConnectionState, lastContactPlaneTimestamp,
                                   gpsConnectionBase, gpsConnectionPlane, barometerConnectionBase,
                                   barometerConnectionPlane, motorComConnectionPlane, magnetometerConnectionPlane,
                                   accelerometerConnectionPlane, manualOverridePlane, flightState)
};

struct AreaDefinePacket : public BaseUpdatePacket {
    std::vector<Coordinate> shape;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AreaDefinePacket, planeId, shape)
};

struct SensorPacket : public BaseUpdatePacket {
    static constexpr const char* type = "sensor";
    float barometerPressureBase;
    float barometerPressurePlane;
    float calculatedAltitude;
    int headingPlane;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SensorPacket, type, planeId, barometerPressureBase, barometerPressurePlane,
                                   calculatedAltitude, headingPlane)
};

struct BatteryStatusPacket : public BaseUpdatePacket {
    static constexpr const char* type = "battery";
    int baseBatteryPercentage;
    int planeBatteryPercentage;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BatteryStatusPacket, type, planeId, baseBatteryPercentage, planeBatteryPercentage)
};

struct PlannedRoutePacket : public BaseUpdatePacket {
    static constexpr const char* type = "plannedRoute";
    std::vector<Coordinate> route;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(PlannedRoutePacket, type, planeId, route)
};

} // namespace Frontend
