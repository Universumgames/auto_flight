#pragma once
#include "types.hpp"
#include <time.h>

class FlightStorageClass {
private:
    FlightStorageClass();

public:
    ~FlightStorageClass() = delete;

public:
    static FlightStorageClass* getInstancePtr();
    static FlightStorageClass& getInstance();

private:
    PlannedRoute latestPlannedRoute;
    time_t lastPlannedRouteUpdateTime = 0;
    FlightRoute latestFlightRoute;
    time_t lastFlightRouteUpdateTime = 0;

    Coordinate basePosition = COORDINATE_INIT_INVALID();
    time_t lastBasePositionUpdateTime = 0;
    Coordinate currentPlanePosition = COORDINATE_INIT_INVALID();
    time_t lastCurrentPlanePositionUpdateTime = 0;

    ConnectionState baseConnectionState = ConnectionState::CONNECTING;
    time_t lastBaseConnectedTime = 0;
    ConnectionState planeConnectionState = ConnectionState::CONNECTING;
    time_t lastPlaneConnectedTime = 0;

public:
    /**
     * Store new planned route and update the last update time. If lastUpdateTime is 0, the current GPS time will be used as the update time.
     * @param plannedRoute the planned route to fly
     * @param lastUpdateTime the timestamp of the update
     */
    void updatePlannedRoute(PlannedRoute plannedRoute, time_t lastUpdateTime = 0);

    /**
     * Store update flight route and update last update time. If lastUpdateTime is 0, the current GPS time will be used as the update time.
     * @param flightRoute the flown route
     * @param lastUpdateTime the timestamp of the update
     */
    void updateFlightRoute(FlightRoute flightRoute, time_t lastUpdateTime = 0);

    /**
     * Get latest stored planned route
     * @return planned route
     */
    PlannedRoute getPlannedRoute();

    /**
     * Get timestamp of planned route update
     * @return gps timestamp of update or 0 if uninitialized
     */
    [[nodiscard]] time_t getLastPlannedRouteUpdateTime() const;

    /**
     * Get latest stored flown route
     * @return flown route
     */
    FlightRoute getFlightRoute();

    /**
     * Get timestamp of last flown update
     * @return gps timestamp of update or 0 if uninitialized
     */
    [[nodiscard]] time_t getLastFlightRouteUpdateTime() const;

    void updateBasePosition(Coordinate basePosition, time_t lastUpdateTime = 0);
    [[nodiscard]] Coordinate getBasePosition() const;
    [[nodiscard]] time_t getLastBasePositionUpdateTime() const;
    void updatePlanePosition(Coordinate currentPlanePosition, time_t lastUpdateTime = 0);
    [[nodiscard]] Coordinate getLastPlanePosition() const;
    [[nodiscard]] time_t getLastPlanePositionUpdateTime() const;

    [[nodiscard]] ConnectionState getPlaneConnectionState() const;
    [[nodiscard]] ConnectionState getBaseStationConnectionState() const;

    [[nodiscard]] time_t getLastConnectionTimestampPlane() const;
    [[nodiscard]] time_t getLastConnectionTimestampBaseStation() const;

    void updatePlaneConnectionState(ConnectionState connectionState, time_t lastUpdateTime = 0);
    void updateBaseStationConnectionState(ConnectionState connectionState, time_t lastUpdateTime = 0);
};


extern FlightStorageClass& FlightStorage;
