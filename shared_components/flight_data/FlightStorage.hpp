#pragma once
#include "types.hpp"
#include <cstdint>
#include <ctime>

#include <functional>
#include <unordered_map>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "RouteData.hpp"

class FlightStorageClass {
private:
    FlightStorageClass();

public:
    ~FlightStorageClass() = delete;

public:
    static FlightStorageClass* getInstancePtr();
    static FlightStorageClass& getInstance();

    void init();

    /// sourceId used for updates that don't originate from a specific plane (base station state)
    static constexpr uint32_t NO_PLANE_ID = 0;
    static constexpr uint32_t BASE_ID = NO_PLANE_ID;

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
        /// Area settings
        AREA_SETTINGS,
        /// Sensor changes
        SENSOR,
        /// Battery changes
        BATTERY,
        /// Flight state changes (planning/planned/flying/returning)
        STATE
    };

    void registerEventHandlersInSubComponents();
    /// callback receives the id of the plane the update is about, or NO_PLANE_ID for base/general updates
    void registerDataChangeCallback(std::function<void(uint32_t)> callback, DataUpdateType type = DataUpdateType::ANY);

    /// State about the base station (there is exactly one).
    struct BaseInfo {
#define FLIGHT_VAR_BASE(name, Name, type, initialValue, updateType, additionalCalls) \
        type name = initialValue; time_t last##Name##UpdateTime = 0;
#include "BaseVariables.inc"
#undef FLIGHT_VAR_BASE
    };

    /// State about a single plane. Stored per plane id in `planes`.
    struct PlaneInfo {
#define FLIGHT_VAR_PLANE(name, Name, type, initialValue, updateType, additionalCalls) \
        type name = initialValue; time_t last##Name##UpdateTime = 0;
#include "PlaneVariables.inc"
#undef FLIGHT_VAR_PLANE
    };

#define FLIGHT_VAR_BASE(name, Name, type, initialValue, updateType, additionalCalls) \
    public: type update##Name(type value, time_t lastUpdateTime = 0); \
            [[nodiscard]] type get##Name() const; \
            [[nodiscard]] time_t getLast##Name##UpdateTime() const;
#include "BaseVariables.inc"
#undef FLIGHT_VAR_BASE

    // Every plane variable gets an explicit-id accessor (used to address any plane by id,
    // e.g. from the base station, which may track several). On plane firmware builds, an
    // additional zero-arg "self" overload is generated that implicitly targets this device's
    // own id, so plane-side code doesn't need to know/pass its own id around.
#define FLIGHT_VAR_PLANE_COMMON(name, Name, type, initialValue, updateType, additionalCalls) \
    public: type update##Name(uint32_t planeId, type value, time_t lastUpdateTime = 0); \
            [[nodiscard]] type get##Name(uint32_t planeId) const; \
            [[nodiscard]] time_t getLast##Name##UpdateTime(uint32_t planeId) const;

#ifdef FLIGHT_DEVICE_TYPE_PLANE
#define FLIGHT_VAR_PLANE_SELF(name, Name, type, initialValue, updateType, additionalCalls) \
    public: type update##Name(type value, time_t lastUpdateTime = 0); \
            [[nodiscard]] type get##Name() const; \
            [[nodiscard]] time_t getLast##Name##UpdateTime() const;
#else
#define FLIGHT_VAR_PLANE_SELF(name, Name, type, initialValue, updateType, additionalCalls)
#endif

#define FLIGHT_VAR_PLANE(name, Name, type, initialValue, updateType, additionalCalls) \
    FLIGHT_VAR_PLANE_COMMON(name, Name, type, initialValue, updateType, additionalCalls) \
    FLIGHT_VAR_PLANE_SELF(name, Name, type, initialValue, updateType, additionalCalls)
#include "PlaneVariables.inc"
#undef FLIGHT_VAR_PLANE
#undef FLIGHT_VAR_PLANE_SELF
#undef FLIGHT_VAR_PLANE_COMMON

    // Universal accessors: dispatch purely on `sourceId`. BASE_ID proxies to the
    // base implementation, any other id to the plane implementation for that id.
    // See UniversalVariables.inc for which variables get one and why.
#define FLIGHT_VAR_UNIVERSAL(name, Name, type, BaseName, PlaneName) \
    public: type update##Name(uint32_t sourceId, type value, time_t lastUpdateTime = 0); \
            [[nodiscard]] type get##Name(uint32_t sourceId) const; \
            [[nodiscard]] time_t getLast##Name##UpdateTime(uint32_t sourceId) const;
#define FLIGHT_VAR_UNIVERSAL_PLANE_ONLY(name, Name, type, initialValue, PlaneName) \
    public: type update##Name(uint32_t sourceId, type value, time_t lastUpdateTime = 0); \
            [[nodiscard]] type get##Name(uint32_t sourceId) const; \
            [[nodiscard]] time_t getLast##Name##UpdateTime(uint32_t sourceId) const;
#include "UniversalVariables.inc"
#undef FLIGHT_VAR_UNIVERSAL_PLANE_ONLY
#undef FLIGHT_VAR_UNIVERSAL

    [[nodiscard]] const BaseInfo& getBaseInfo() const { return baseInfo; }
    [[nodiscard]] const std::unordered_map<uint32_t, PlaneInfo>& getAllPlanes() const { return planes; }

    [[nodiscard]] const PlaneInfo* getPlaneInfo(uint32_t planeId) const {
        auto it = planes.find(planeId);
        return it != planes.end() ? &it->second : nullptr;
    }

private:
    void addPointToFlightRoute(const Coordinate &point, uint32_t planeId);

    void queueUpdate(DataUpdateType type, uint32_t sourceId = NO_PLANE_ID, TickType_t timeout = pdMS_TO_TICKS(2));

    QueueHandle_t dataUpdateQueue;

    struct DataUpdateEvent {
        DataUpdateType type;
        /// NO_PLANE_ID for base/general updates, otherwise the plane's device id
        uint32_t sourceId;
    };

    BaseInfo baseInfo;
    std::unordered_map<uint32_t, PlaneInfo> planes;

    std::unordered_map<DataUpdateType,std::vector<std::function<void(uint32_t)>>> dataChangeCallbacks;

    void callDataChangeCallbacks(DataUpdateType type, uint32_t sourceId);
    static void callDataChangeCallbacksTaskEntry(void* args);

    static void callbackLoopEntry(void* param);

    [[noreturn]] void callbackLoop();

};

extern FlightStorageClass& FlightStorage;
