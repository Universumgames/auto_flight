#pragma once
#include <vector>
#include "types.hpp"

namespace Frontend{
    enum class PacketType: uint8_t {
        UNDEFINED_PACKET = 255,
        FLIGHT_UPDATE = 0x00,
        CONNECTION_UPDATE = 0x01,
        AREA_DEFINE = 0x02,
        SENSOR_UPDATE = 0x03,
        BATTERY_STATUS = 0x04,
        PLANNED_ROUTE = 0x05
    };


    /// Common base for packets exchanged with the frontend that concern a specific plane. Carries
    /// the plane's device id as real, explicit protocol data, so neither side has to guess which
    /// plane an update is about (mirrors BasePacket::id in flight_com).
    struct BaseUpdatePacket {
        static constexpr PacketType IDENTIFIER = PacketType::UNDEFINED_PACKET;
        uint32_t planeId = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BaseUpdatePacket, planeId)
    };

    struct FlightUpdatePacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::FLIGHT_UPDATE;
        Coordinate basePosition;
        time_t basePositionUpdateTime;
        Coordinate planePosition;
        time_t planePositionUpdateTime;
        FlightRoute flightRoute;
        time_t flightRouteUpdateTime;
        PlannedRoute plannedRoute;
        time_t plannedRouteUpdateTime;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(FlightUpdatePacket, planeId, basePosition, basePositionUpdateTime,
                                       planePosition, planePositionUpdateTime, flightRoute, flightRouteUpdateTime,
                                       plannedRoute, plannedRouteUpdateTime)
    };

    struct ConnectionUpdatePacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::CONNECTION_UPDATE;
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

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionUpdatePacket, planeId, baseConnectionState,
                                       lastContactBaseStationTimestamp, planeConnectionState, lastContactPlaneTimestamp,
                                       gpsConnectionBase, gpsConnectionPlane, barometerConnectionBase,
                                       barometerConnectionPlane, motorComConnectionPlane, magnetometerConnectionPlane,
                                       accelerometerConnectionPlane, manualOverridePlane, flightState)
    };

    struct AreaDefinePacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::AREA_DEFINE;
        std::vector<Coordinate> shape;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(AreaDefinePacket, planeId, shape)
    };

    struct SensorPacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::SENSOR_UPDATE;
        float barometerPressureBase;
        float barometerPressurePlane;
        float calculatedAltitude;
        int headingPlane;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SensorPacket, planeId, barometerPressureBase, barometerPressurePlane,
                                       calculatedAltitude, headingPlane)
    };

    struct BatteryStatusPacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::BATTERY_STATUS;
        int baseBatteryPercentage;
        int planeBatteryPercentage;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BatteryStatusPacket, planeId, baseBatteryPercentage, planeBatteryPercentage)
    };

    struct PlannedRoutePacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::PLANNED_ROUTE;
        std::vector<Coordinate> route;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PlannedRoutePacket, planeId, route)
    };
} // namespace Frontend
