#pragma once
#include "types.hpp"
#include <ctime>

#include <functional>
#include <unordered_map>
#include <vector>

#include "freertos/FreeRTOS.h"

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
        /// Flight History/route changes
        HISTORY,
        /// area changes
        AREA,
        /// Sensor changes
        SENSOR,
        /// Flight state changes (planning/planned/flying/returning)
        STATE
    };

    void registerEventHandlersInSubComponents();
    void registerDataChangeCallback(std::function<void()> callback, DataUpdateType type = DataUpdateType::ANY);

#define FLIGHT_VAR(name, Name, type, initialValue, updateType, additionalCalls) \
    private: type name = initialValue; time_t last##Name##UpdateTime = 0; \
    public:  type update##Name(type value, time_t lastUpdateTime = 0); \
             [[nodiscard]] type get##Name() const; \
             [[nodiscard]] time_t getLast##Name##UpdateTime() const;
#include "FlightVariables.inc"
#undef FLIGHT_VAR

private:
    void addPointToFlightRoute(const Coordinate &point);

    QueueHandle_t dataUpdateQueue;

    std::unordered_map<DataUpdateType,std::vector<std::function<void()>>> dataChangeCallbacks;

    void callDataChangeCallbacks(DataUpdateType type);
    static void callDataChangeCallbacksTaskEntry(void* args);

    static void callbackLoopEntry(void* param);

    [[noreturn]] void callbackLoop();

};

extern FlightStorageClass& FlightStorage;
