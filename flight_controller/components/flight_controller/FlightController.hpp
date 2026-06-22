#pragma once
#include "Gyroscope.hpp"
#include "LoRa_Communication.hpp"

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

    void communicationCallback(LoRaPacket packet);

    static void flightTaskEntry(void* param);

    [[noreturn]] void flightTask();

    static void sendUpdateTaskEntry(void* param);

    [[noreturn]] void sendUpdateTask();

    void recalculateRoute();

    void steerToWaypoint(Coordinate waypoint, int height, GyroscopeClass::PlaneAngle angle);


};


extern FlightControllerClass& FlightController;