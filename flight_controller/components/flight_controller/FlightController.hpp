#pragma once
#include "Gyroscope.hpp"
#include "LoRa_Communication.hpp"
#include "MovingAverage.hpp"
#include "RouteData.hpp"
#include "SteeringLaw.hpp"

class FlightControllerClass {
private:
    FlightControllerClass();

    static const char* TAG_FLIGHT_CONTROLLER;
public:
    ~FlightControllerClass() = delete;

    static FlightControllerClass& getInstance();
    static FlightControllerClass* getInstancePtr();

    void init();

    Coordinate getNextWaypoint() const;

private:

    bool plannedAreaChanged = false;
    size_t nextWaypointIndex = 0;

    float targetAltitude = 60.0f; // meters; matches route planner cruise altitude

    MovingAverage<CONFIG_ALTITUDE_SMOOTHING_SAMPLES> altitudeAverage;
    MovingAverage<CONFIG_ROLL_SMOOTHING_SAMPLES> rollAverage;
    MovingAverage<CONFIG_PITCH_SMOOTHING_SAMPLES> pitchAverage;
    HeadingMovingAverage<CONFIG_HEADING_SMOOTHING_SAMPLES> headingAverage;

    AltitudeHoldController altitudeHold;
    AttitudeController attitude;

    void communicationCallback(LoRaPacket packet);

    static void flightTaskEntry(void* param);

    [[noreturn]] void flightTask();

    static void sendUpdateTaskEntry(void* param);

    [[noreturn]] void sendUpdateTask();

    void recalculateRoute(const AreaData& newPlannedArea);

    void checkAndAdvanceWaypoint(Coordinate currentPosition);

    void steerToWaypoint(Coordinate waypoint, int height, GyroscopeClass::GroundAngle angle, float compassHeading);


};


extern FlightControllerClass& FlightController;