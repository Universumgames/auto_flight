#pragma once
#include "Gyroscope.hpp"
#include "LoRa_Communication.hpp"
#include "MovingAverage.hpp"

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

    float aileronIntegral = 0.0f;
    float pitchIntegral   = 0.0f;
    float thrustIntegral  = 0.0f;

    float targetAltitude = 60.0f; // meters; matches route planner cruise altitude

    MovingAverage<CONFIG_ALTITUDE_SMOOTHING_SAMPLES> altitudeAverage;
    MovingAverage<CONFIG_ROLL_SMOOTHING_SAMPLES> rollAverage;
    MovingAverage<CONFIG_PITCH_SMOOTHING_SAMPLES> pitchAverage;
    HeadingMovingAverage<CONFIG_HEADING_SMOOTHING_SAMPLES> headingAverage;

    static constexpr float AILERON_P_GAIN = 1.5f;
    static constexpr float AILERON_I_GAIN = 0.5f;
    static constexpr float AILERON_I_LIMIT = 30.0f;

    static constexpr float PITCH_P_GAIN = 1.5f;
    static constexpr float PITCH_I_GAIN = 0.5f;
    static constexpr float PITCH_I_LIMIT = 30.0f;

    static constexpr float ALTITUDE_TO_PITCH_GAIN = 0.3f;  // deg of target pitch per meter of altitude error
    static constexpr float MAX_CLIMB_PITCH        = 12.0f; // clamp on target pitch degrees

    static constexpr float THRUST_BASE    = 50.0f;
    static constexpr float THRUST_P_GAIN  = 0.5f;
    static constexpr float THRUST_I_GAIN  = 0.3f;
    static constexpr float THRUST_I_LIMIT = 20.0f;

    void communicationCallback(LoRaPacket packet);

    static void flightTaskEntry(void* param);

    [[noreturn]] void flightTask();

    static void sendUpdateTaskEntry(void* param);

    [[noreturn]] void sendUpdateTask();

    void recalculateRoute();

    void checkAndAdvanceWaypoint(Coordinate currentPosition);

    void steerToWaypoint(Coordinate waypoint, int height, GyroscopeClass::GroundAngle angle, float compassHeading);


};


extern FlightControllerClass& FlightController;