#pragma once
#include <vector>

#include "FlightStorage.hpp"
#include "types.hpp"

namespace Frontend{
    enum class PacketType: uint8_t {
        UNDEFINED_PACKET = static_cast<uint8_t>(FlightStorageClass::DataUpdateType::ANY),
        POSITION_UPDATE = static_cast<uint8_t>(FlightStorageClass::DataUpdateType::POSITION),
        CONNECTION_UPDATE = static_cast<uint8_t>(FlightStorageClass::DataUpdateType::CONNECTION),
        AREA_DEFINE = static_cast<uint8_t>(FlightStorageClass::DataUpdateType::AREA),
        SENSOR_UPDATE = static_cast<uint8_t>(FlightStorageClass::DataUpdateType::SENSOR),
        BATTERY_STATUS = static_cast<uint8_t>(FlightStorageClass::DataUpdateType::BATTERY),
        PLANNED_ROUTE = static_cast<uint8_t>(FlightStorageClass::DataUpdateType::ROUTE)
    };


    /// Common base for packets exchanged with the frontend that concern a specific plane. Carries
    /// the plane's device id as real, explicit protocol data, so neither side has to guess which
    /// plane an update is about (mirrors BasePacket::id in flight_com).
    struct BaseUpdatePacket {
        static constexpr PacketType IDENTIFIER = PacketType::UNDEFINED_PACKET;
        uint32_t sourceId = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BaseUpdatePacket, sourceId)
    };

    struct PositionUpdatePacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::POSITION_UPDATE;
        Coordinate position;
        time_t positionUpdateTime;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PositionUpdatePacket, sourceId, position, positionUpdateTime)
    };

    struct ConnectionUpdatePacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::CONNECTION_UPDATE;

        // Common components
        ConnectionState gpsConnection;
        ConnectionState barometer;

        // Plane-specific components
        ConnectionState motorCom;
        ConnectionState magnetometer;
        ConnectionState accelerometer;
        bool manualOverride;
        FlightState flightState;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConnectionUpdatePacket, sourceId, gpsConnection, barometer,
                                       motorCom, magnetometer, accelerometer, manualOverride, flightState)
    };

    struct AreaDefinePacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::AREA_DEFINE;
        std::vector<Coordinate> shape;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(AreaDefinePacket, sourceId, shape)
    };

    struct SensorPacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::SENSOR_UPDATE;

        // Common components
        float barometerPressure;
        float calculatedAltitude;

        // Plane-specific components
        int heading;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SensorPacket, sourceId, barometerPressure,
                                       calculatedAltitude, heading)
    };

    struct BatteryStatusPacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::BATTERY_STATUS;
        int batteryPercentage;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BatteryStatusPacket, sourceId, batteryPercentage)
    };

    struct PlannedRoutePacket : public BaseUpdatePacket {
        static constexpr auto IDENTIFIER = PacketType::PLANNED_ROUTE;
        std::vector<Coordinate> route;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(PlannedRoutePacket, sourceId, route)
    };
} // namespace Frontend
