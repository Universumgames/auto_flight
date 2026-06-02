#include "FlightStorage.hpp"

#include <utility>

#include "GPS_Reader.hpp"
#include "helper.hpp"

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
    dataUpdateQueue = xQueueCreate(20, sizeof(DataUpdateType));
    queueMutex = xSemaphoreCreateMutex();

    registerEventHandlersInSubComponents();

    xTaskCreate(callbackLoopEntry, "FlightStorageCallbackLoop", 4096, this, tskIDLE_PRIORITY + 1, nullptr);
}

void FlightStorageClass::registerDataChangeCallback(std::function<void()> callback, DataUpdateType type) {
    dataChangeCallbacks[type].push_back(std::move(callback));
}

void FlightStorageClass::registerEventHandlersInSubComponents() {
    GPS_Reader.addPositionUpdateCallback([this](Coordinate newPosition, time_t updateTime) {
       //ESP_LOGW(__FUNCTION__, "Position update callback triggered with new position: (%.6f, %.6f) at time %ld",
       //          newPosition.latitude, newPosition.longitude, updateTime);
        if (isDeviceBaseStation()) {
            updateBasePosition(newPosition, updateTime);
        }
        else if (isDevicePlane()) {
            updatePlanePosition(newPosition, updateTime);
        }
    });
}

void FlightStorageClass::callDataChangeCallbacks(DataUpdateType type) {
    for (const auto& callback_fn : dataChangeCallbacks[type]) {
        callback_fn();
    }
    for (const auto& callback_fn : dataChangeCallbacks[DataUpdateType::ANY]) {
        callback_fn();
    }
}

void FlightStorageClass::callbackLoopEntry(void* param) {
    FlightStorageClass* instance = static_cast<FlightStorageClass*>(param);
    instance->callbackLoop();
}

[[noreturn]] void FlightStorageClass::callbackLoop() {
    DataUpdateType updateType;
    while (true) {
        WITH_MUTEX(queueMutex) {
            if (xQueueReceive(dataUpdateQueue, &updateType, portMAX_DELAY) == pdTRUE) {
                callDataChangeCallbacks(updateType);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#define FLIGHT_VARIABLE_IMPL_COMPLEX(name, Name, type, updateType, additionalCalls)\
    time_t FlightStorageClass::getLast##Name##UpdateTime() const {\
        return last##Name##UpdateTime;\
    }\
    type FlightStorageClass::get##Name() const {\
        return name;\
    }\
    type FlightStorageClass::update##Name(type value, time_t lastUpdateTime) {\
        if (lastUpdateTime == 0) {\
            lastUpdateTime = GPS_Reader.getGPSLatestTime();\
        }\
        this->name = value;\
        this->last##Name##UpdateTime = lastUpdateTime;\
        auto updateTypeVar = DataUpdateType::updateType;\
        {additionalCalls;}\
        WITH_MUTEX(queueMutex) \
        xQueueSend(dataUpdateQueue, &updateTypeVar, portMAX_DELAY);\
        return value;\
    }

#define FLIGHT_VARIABLE_IMPL(name, Name, type, updateType) FLIGHT_VARIABLE_IMPL_COMPLEX(name, Name, type, updateType, {})

FLIGHT_VARIABLE_IMPL(plannedRoute, PlannedRoute, PlannedRoute, ROUTE)
FLIGHT_VARIABLE_IMPL(flightRoute, FlightRoute, FlightRoute, ROUTE)

FLIGHT_VARIABLE_IMPL(basePosition, BasePosition, Coordinate, POSITION)
FLIGHT_VARIABLE_IMPL(planePosition, PlanePosition, Coordinate, POSITION)

FLIGHT_VARIABLE_IMPL(baseConnectionState, BaseConnectionState, ConnectionState, CONNECTION)
FLIGHT_VARIABLE_IMPL(planeConnectionState, PlaneConnectionState, ConnectionState, CONNECTION)

FLIGHT_VARIABLE_IMPL(basePressure, BasePressure, float, SENSOR)
FLIGHT_VARIABLE_IMPL(planePressure, PlanePressure, float, SENSOR)

FLIGHT_VARIABLE_IMPL_COMPLEX(plannedArea, PlannedArea, std::vector<Coordinate>, AREA, {plannedRoute.clear();})

