#include "FlightStorage.hpp"

#include <utility>

#include "DeviceId.hpp"
#include "GPS_Reader.hpp"
#include "helper.hpp"
#include "geo_helper.hpp"

#define WITH_MUTEX_CUSTOM_DELAY(mutex, delay)        \
for (bool _once = (xSemaphoreTake((mutex), delay) == pdTRUE); \
_once; \
_once = false, xSemaphoreGive((mutex)))

#define WITH_MUTEX(mutex) \
WITH_MUTEX_CUSTOM_DELAY(mutex, portMAX_DELAY)

static FlightStorageClass* flightStorageInstance = nullptr;

FlightStorageClass& FlightStorage = FlightStorageClass::getInstance();

FlightStorageClass::FlightStorageClass() = default;

FlightStorageClass* FlightStorageClass::getInstancePtr() {
    if (!flightStorageInstance) {
        flightStorageInstance = new FlightStorageClass();
    }
    return flightStorageInstance;
}

FlightStorageClass& FlightStorageClass::getInstance() {
    return *getInstancePtr();
}

void FlightStorageClass::init() {
    dataUpdateQueue = xQueueCreate(50, sizeof(DataUpdateEvent));
    if (dataUpdateQueue == nullptr) {
        ESP_LOGE("FlightStorage", "Failed to create data update queue");
        return;
    }

    registerEventHandlersInSubComponents();

    xTaskCreate(callbackLoopEntry, "FlightStorageCallbackLoop", 4096, this, tskIDLE_PRIORITY + 1, nullptr);
}

void FlightStorageClass::registerDataChangeCallback(std::function<void(uint32_t)> callback, DataUpdateType type) {
    dataChangeCallbacks[type].push_back(std::move(callback));
}

void FlightStorageClass::registerEventHandlersInSubComponents() {
    GPS_Reader.addPositionUpdateCallback([this](Coordinate newPosition, time_t updateTime) {
        //ESP_LOGW(__FUNCTION__, "Position update callback triggered with new position: (%.6f, %.6f) at time %ld",
        //          newPosition.latitude, newPosition.longitude, updateTime);
#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
        updateBasePosition(newPosition, updateTime);
        updateBaseGPSConnectionState(GPS_Reader.hasValidPosition()
                                         ? ConnectionState::CONNECTED
                                         : ConnectionState::CONNECTING);
#elif defined(FLIGHT_DEVICE_TYPE_PLANE)
        updatePlanePosition(newPosition, updateTime);
        updatePlaneGPSConnectionState(GPS_Reader.hasValidPosition()
                                          ? ConnectionState::CONNECTED
                                          : ConnectionState::CONNECTING);
#endif
    });
}

void FlightStorageClass::queueUpdate(DataUpdateType type, uint32_t sourceId, TickType_t timeout) {
    DataUpdateEvent event{type, sourceId};
    xQueueSendToBack(dataUpdateQueue, &event, timeout);
}

void FlightStorageClass::callDataChangeCallbacks(DataUpdateType type, uint32_t sourceId) {
    for (const auto& callback_fn : dataChangeCallbacks[type]) {
        callback_fn(sourceId);
    }
    for (const auto& callback_fn : dataChangeCallbacks[DataUpdateType::ANY]) {
        callback_fn(sourceId);
    }
}

void FlightStorageClass::callDataChangeCallbacksTaskEntry(void* args) {
    auto* event = static_cast<DataUpdateEvent*>(args);
    getInstancePtr()->callDataChangeCallbacks(event->type, event->sourceId);
    vTaskDelete(nullptr);
}

void FlightStorageClass::callbackLoopEntry(void* param) {
    auto* instance = static_cast<FlightStorageClass*>(param);
    instance->callbackLoop();
}

[[noreturn]] void FlightStorageClass::callbackLoop() {
    DataUpdateEvent event;
    while (true) {
        if (xQueueReceive(dataUpdateQueue, &event, portMAX_DELAY) == pdTRUE) {
            //xTaskCreate(callbackLoopEntry, "FlightStorageCallbackLoop", 8192, &event, 10, nullptr);
            callDataChangeCallbacks(event.type, event.sourceId);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Base station variables — stored in `baseInfo`.
#define FLIGHT_VAR_BASE(name, Name, type, initialValue, updateType, additionalCalls)\
    time_t FlightStorageClass::getLast##Name##UpdateTime() const {\
        return baseInfo.last##Name##UpdateTime;\
    }\
    type FlightStorageClass::get##Name() const {\
        return baseInfo.name;\
    }\
    type FlightStorageClass::update##Name(type value, time_t lastUpdateTime) {\
        if (lastUpdateTime == 0) {\
            lastUpdateTime = GPS_Reader.getGPSLatestTime();\
        }\
        if (baseInfo.name == value)\
            return value;\
        [[maybe_unused]] auto originalValue = baseInfo.name;\
        baseInfo.name = value;\
        baseInfo.last##Name##UpdateTime = lastUpdateTime;\
        auto updateTypeVar = DataUpdateType::updateType;\
        additionalCalls\
        queueUpdate(updateTypeVar);\
        return value;\
    }
#include "BaseVariables.inc"
#undef FLIGHT_VAR_BASE

// Plane variables — stored per plane id in `planes`. Every variable gets an explicit-id
// accessor; plane firmware builds additionally get a zero-arg "self" overload that forwards
// to the explicit-id one using this device's own id.
#define FLIGHT_VAR_PLANE_COMMON(name, Name, type, initialValue, updateType, additionalCalls)\
    time_t FlightStorageClass::getLast##Name##UpdateTime(uint32_t planeId) const {\
        auto it = planes.find(planeId);\
        return it == planes.end() ? 0 : it->second.last##Name##UpdateTime;\
    }\
    type FlightStorageClass::get##Name(uint32_t planeId) const {\
        auto it = planes.find(planeId);\
        return it == planes.end() ? type(initialValue) : it->second.name;\
    }\
    type FlightStorageClass::update##Name(uint32_t planeId, type value, time_t lastUpdateTime) {\
        if (lastUpdateTime == 0) {\
            lastUpdateTime = GPS_Reader.getGPSLatestTime();\
        }\
        auto& info = planes[planeId];\
        if (info.name == value)\
            return value;\
        [[maybe_unused]] auto originalValue = info.name;\
        info.name = value;\
        info.last##Name##UpdateTime = lastUpdateTime;\
        auto updateTypeVar = DataUpdateType::updateType;\
        additionalCalls\
        queueUpdate(updateTypeVar, planeId);\
        return value;\
    }

#ifdef FLIGHT_DEVICE_TYPE_PLANE
#define FLIGHT_VAR_PLANE_SELF(name, Name, type, initialValue, updateType, additionalCalls)\
    time_t FlightStorageClass::getLast##Name##UpdateTime() const {\
        return getLast##Name##UpdateTime(DeviceId::get32());\
    }\
    type FlightStorageClass::get##Name() const {\
        return get##Name(DeviceId::get32());\
    }\
    type FlightStorageClass::update##Name(type value, time_t lastUpdateTime) {\
        return update##Name(DeviceId::get32(), value, lastUpdateTime);\
    }
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

// Universal accessors — dispatch to the base or plane implementation based on
// sourceId (see UniversalVariables.inc).
#define FLIGHT_VAR_UNIVERSAL(name, Name, type, BaseName, PlaneName)\
    time_t FlightStorageClass::getLast##Name##UpdateTime(uint32_t sourceId) const {\
        return sourceId == BASE_ID ? getLast##BaseName##UpdateTime() : getLast##PlaneName##UpdateTime(sourceId);\
    }\
    type FlightStorageClass::get##Name(uint32_t sourceId) const {\
        return sourceId == BASE_ID ? get##BaseName() : get##PlaneName(sourceId);\
    }\
    type FlightStorageClass::update##Name(uint32_t sourceId, type value, time_t lastUpdateTime) {\
        return sourceId == BASE_ID ? update##BaseName(value, lastUpdateTime) : update##PlaneName(sourceId, value, lastUpdateTime);\
    }
#define FLIGHT_VAR_UNIVERSAL_PLANE_ONLY(name, Name, type, initialValue, PlaneName)\
    time_t FlightStorageClass::getLast##Name##UpdateTime(uint32_t sourceId) const {\
        return sourceId == BASE_ID ? 0 : getLast##PlaneName##UpdateTime(sourceId);\
    }\
    type FlightStorageClass::get##Name(uint32_t sourceId) const {\
        return sourceId == BASE_ID ? type(initialValue) : get##PlaneName(sourceId);\
    }\
    type FlightStorageClass::update##Name(uint32_t sourceId, type value, time_t lastUpdateTime) {\
        return sourceId == BASE_ID ? type(initialValue) : update##PlaneName(sourceId, value, lastUpdateTime);\
    }
#include "UniversalVariables.inc"
#undef FLIGHT_VAR_UNIVERSAL_PLANE_ONLY
#undef FLIGHT_VAR_UNIVERSAL

void FlightStorageClass::addPointToFlightRoute(const Coordinate& point, uint32_t planeId) {
    planes[planeId].flightRoute.push_back(point);
    queueUpdate(DataUpdateType::HISTORY, planeId, portMAX_DELAY);
}
