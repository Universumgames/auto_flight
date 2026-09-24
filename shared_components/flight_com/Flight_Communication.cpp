#include "Flight_Communication.hpp"

#include "Barometer.hpp"
#include "FlightStorage.hpp"
#include "GPS_Reader.hpp"
#include "LoRa_Communication.hpp"
#ifdef FLIGHT_DEVICE_TYPE_PLANE
#include "MotorComMaster.hpp"
#include "Magnetometer.hpp"
#endif
#include <cmath>

#include "Battery.hpp"

void Flight_Communication::begin() {}

void Flight_Communication::sendPacket(const BasePacket& packet) {
    auto data = packet.serialize();
    LoRa_Communication.sendData(data.first.get(), data.second);
}

void Flight_Communication::sendPosition() {
    Coordinate position = GPS_Reader.getCurrentPosition();
    PositionUpdate packet = {
        GPS_Reader.getGPSLatestTime(),
        position
    };
    sendPacket(packet);
}

void Flight_Communication::sendSensorUpdate() {
    float pressure = Barometer.getPressure();
#ifdef FLIGHT_DEVICE_TYPE_PLANE
    int heading = static_cast<int>(std::lround(Magnetometer.getHeading()));
#else
    int heading = 0;
#endif
    SensorUpdate packet = {
        GPS_Reader.getGPSLatestTime(),
        pressure,
        heading,
        FlightStorage.getBaseBatteryPercentage()
    };

    sendPacket(packet);
}

#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
void Flight_Communication::requestRouteHistory() {
    BasePacket packet = {
        GPS_Reader.getGPSLatestTime(),
        PacketType::ROUTE_HISTORY_REQUEST
    };

    sendPacket(packet);
}

void Flight_Communication::sendPlannedArea(const AreaData& areaData) {
    PlannedAreaPacket packet = {
         GPS_Reader.getGPSLatestTime(),
        areaData.areaPoints,
        areaData.settings
    };
    sendPacket(packet);
}
#endif

#ifdef FLIGHT_DEVICE_TYPE_PLANE
void Flight_Communication::sendRouteHistory() {
    FlightRoute flightRoute = FlightStorage.getFlightRoute();
    FlightHistoryPacket packet = {
        GPS_Reader.getGPSLatestTime(),
        flightRoute
    };
    sendPacket(packet);
}

void Flight_Communication::sendPlannedRoute() {
    auto plannedRoute = FlightStorage.getPlannedRoute();
    PlannedRoutePacket packet = {
        GPS_Reader.getGPSLatestTime(),
        plannedRoute.getRoutePoints(),
        plannedRoute.getSettings()
    };
    sendPacket(packet);
}

void Flight_Communication::sendComponentStatus() {
    ComponentStatus status = {
    };
    status.timestamp = GPS_Reader.getGPSLatestTime();
    status.type = PacketType::COMPONENT_STATUS;
    status.gps = FlightStorage.getPlaneGPSConnectionState();
    status.barometer = FlightStorage.getPlaneBarometerConnectionState();
    status.motorControl = FlightStorage.getPlaneMotorControlConnectionState();
    status.magnetometer = FlightStorage.getPlaneMagnetometerConnectionState();
    status.accelerometer = FlightStorage.getPlaneAccelerometerConnectionState();
    status.manualOverride = FlightStorage.getPlaneManualOverride();
    status.flightState = FlightStorage.getFlightState();

    sendPacket(status);
}
#endif
