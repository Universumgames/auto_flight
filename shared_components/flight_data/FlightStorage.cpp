#include "FlightStorage.hpp"

#include <utility>

#include "GPS_Reader.hpp"
#include "helper.hpp"

static FlightStorageClass* flightStorageInstance = nullptr;

FlightStorageClass& FlightStorage = FlightStorageClass::getInstance();

FlightStorageClass::FlightStorageClass() {
 GPS_Reader.addPositionUpdateCallback([this](Coordinate newPosition, time_t updateTime) {
     if (isDeviceBaseStation()) {
         updateBasePosition(newPosition, updateTime);
     }else if (isDevicePlane()) {
         updatePlanePosition(newPosition, updateTime);
     }
 });
}

FlightStorageClass* FlightStorageClass::getInstancePtr() {
    if (!flightStorageInstance) {
        flightStorageInstance = new FlightStorageClass();
    }
    return flightStorageInstance;
}

FlightStorageClass& FlightStorageClass::getInstance() {
    return *getInstancePtr();
}

time_t FlightStorageClass::getLastFlightRouteUpdateTime() const {
    return lastFlightRouteUpdateTime;
}

time_t FlightStorageClass::getLastPlannedRouteUpdateTime() const {
    return lastPlannedRouteUpdateTime;
}

FlightRoute FlightStorageClass::getFlightRoute() {
    return latestFlightRoute;
}

PlannedRoute FlightStorageClass::getPlannedRoute() {
    return latestPlannedRoute;
}

void FlightStorageClass::updatePlannedRoute(PlannedRoute plannedRoute, time_t lastUpdateTime) {
    if (lastUpdateTime == 0) {
        lastUpdateTime = GPS_Reader.getGPSLatestTime();
    }
    this->latestPlannedRoute = std::move(plannedRoute);
    this->lastPlannedRouteUpdateTime = lastUpdateTime;
}

void FlightStorageClass::updateFlightRoute(FlightRoute flightRoute, time_t lastUpdateTime) {
    if (lastUpdateTime == 0) {
        lastUpdateTime = GPS_Reader.getGPSLatestTime();
    }
    this->latestFlightRoute = std::move(flightRoute);
    this->lastFlightRouteUpdateTime = lastUpdateTime;
}


void FlightStorageClass::updateBasePosition(Coordinate basePosition, time_t lastUpdateTime) {
    if (lastUpdateTime == 0) {
        lastUpdateTime = GPS_Reader.getGPSLatestTime();
    }
    this->basePosition = basePosition;
    this->lastBasePositionUpdateTime = lastUpdateTime;
}

Coordinate FlightStorageClass::getBasePosition() const {
    return basePosition;
}

time_t FlightStorageClass::getLastBasePositionUpdateTime() const {
    return lastBasePositionUpdateTime;
}

void FlightStorageClass::updatePlanePosition(Coordinate currentPlanePosition, time_t lastUpdateTime) {
    if (lastUpdateTime == 0) {
        lastUpdateTime = GPS_Reader.getGPSLatestTime();
    }
    this->currentPlanePosition = currentPlanePosition;
    this->lastCurrentPlanePositionUpdateTime = lastUpdateTime;
}

Coordinate FlightStorageClass::getLastPlanePosition() const {
    return currentPlanePosition;
}

time_t FlightStorageClass::getLastPlanePositionUpdateTime() const {
    return lastCurrentPlanePositionUpdateTime;
}

ConnectionState FlightStorageClass::getBaseStationConnectionState() const {
    return baseConnectionState;
}

ConnectionState FlightStorageClass::getPlaneConnectionState() const {
    return planeConnectionState;
}

time_t FlightStorageClass::getLastConnectionTimestampPlane() const {
    return lastCurrentPlanePositionUpdateTime;
}

time_t FlightStorageClass::getLastConnectionTimestampBaseStation() const {
    return lastBasePositionUpdateTime;
}

void FlightStorageClass::updatePlaneConnectionState(ConnectionState connectionState, time_t lastUpdateTime) {
    if (lastUpdateTime == 0) {
        lastUpdateTime = GPS_Reader.getGPSLatestTime();
    }
    this->planeConnectionState = connectionState;
    if (connectionState == ConnectionState::CONNECTED)
        this->lastPlaneConnectedTime = lastUpdateTime;
}

void FlightStorageClass::updateBaseStationConnectionState(ConnectionState connectionState, time_t lastUpdateTime) {
    if (lastUpdateTime == 0) {
        lastUpdateTime = GPS_Reader.getGPSLatestTime();
    }
    this->baseConnectionState = connectionState;
    if (connectionState == ConnectionState::CONNECTED)
        this->lastBaseConnectedTime = lastUpdateTime;
}
