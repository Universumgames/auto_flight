#pragma once
#include "types.hpp"
#include <ctime>

#include <functional>
#include <unordered_map>
#include <vector>

#include "freertos/FreeRTOS.h"

/// Helper macro to define a variable, update time, getter and setter
#define FLIGHT_VARIABLE(name, Name, type, initialValue)\
    private:\
        type name = initialValue;\
        time_t last##Name##UpdateTime = 0;\
    public:\
        type update##Name(type value, time_t lastUpdateTime = 0);\
        [[nodiscard]] type get##Name() const;\
        [[nodiscard]] time_t getLast##Name##UpdateTime() const;

class FlightStorageClass {
private:
    FlightStorageClass();

public:
    ~FlightStorageClass() = delete;

public:
    static FlightStorageClass* getInstancePtr();
    static FlightStorageClass& getInstance();

    void init();

    /// Specifying which kind of stored data got updated
    enum class DataUpdateType {
        /// Any update
        ANY,
        /// Updates to position, pressure, rotation
        POSITION,
        /// Connection updates regarding base-plane connection, gps' or other sensors
        CONNECTION,
        /// Route changes
        ROUTE,
        /// area changes
        AREA,
        /// Sensor changes
        SENSOR
    };

    void registerEventHandlersInSubComponents();
    void registerDataChangeCallback(std::function<void()> callback, DataUpdateType type = DataUpdateType::ANY);

private:
    FLIGHT_VARIABLE(plannedRoute, PlannedRoute, PlannedRoute, PlannedRoute{})
    FLIGHT_VARIABLE(flightRoute, FlightRoute, FlightRoute, FlightRoute{})

    FLIGHT_VARIABLE(basePosition, BasePosition, Coordinate, COORDINATE_INIT_INVALID())
    FLIGHT_VARIABLE(planePosition, PlanePosition, Coordinate, COORDINATE_INIT_INVALID())

    FLIGHT_VARIABLE(baseConnectionState, BaseConnectionState, ConnectionState, ConnectionState::CONNECTING)
    FLIGHT_VARIABLE(planeConnectionState, PlaneConnectionState, ConnectionState, ConnectionState::CONNECTING)

    FLIGHT_VARIABLE(basePressure, BasePressure, float, 0.0f)
    FLIGHT_VARIABLE(planePressure, PlanePressure, float, 0.0f)

    FLIGHT_VARIABLE(plannedArea, PlannedArea, std::vector<Coordinate>, std::vector<Coordinate>{})

    FLIGHT_VARIABLE(baseBarometerConnectionState, BaseBarometerConnectionState, ConnectionState, ConnectionState::CONNECTING)
    FLIGHT_VARIABLE(planeBarometerConnectionState, PlaneBarometerConnectionState, ConnectionState, ConnectionState::CONNECTING)
    FLIGHT_VARIABLE(planeGyroscopeConnectionState, PlaneGyroscopeConnectionState, ConnectionState, ConnectionState::CONNECTING)
    FLIGHT_VARIABLE(planeMotorControlConnectionState, PlaneMotorControlConnectionState, ConnectionState, ConnectionState::CONNECTING)

private:

    QueueHandle_t dataUpdateQueue;
    SemaphoreHandle_t queueMutex = nullptr;

    std::unordered_map<DataUpdateType,std::vector<std::function<void()>>> dataChangeCallbacks;

    void callDataChangeCallbacks(DataUpdateType type);

    static void callbackLoopEntry(void* param);

    [[noreturn]] void callbackLoop();

};

extern FlightStorageClass& FlightStorage;
