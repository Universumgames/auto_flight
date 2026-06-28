#include "FlightStorage.hpp"

#include <utility>

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
    dataUpdateQueue = xQueueCreate(50, sizeof(DataUpdateType));
    if (dataUpdateQueue == nullptr) {
        ESP_LOGE("FlightStorage", "Failed to create data update queue");
        return;
    }

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
            updateBaseGPSConnectionState(GPS_Reader.hasValidPosition()
                                             ? ConnectionState::CONNECTED
                                             : ConnectionState::CONNECTING);
        }
        else if (isDevicePlane()) {
            updatePlanePosition(newPosition, updateTime);
            updatePlaneGPSConnectionState(GPS_Reader.hasValidPosition()
                                              ? ConnectionState::CONNECTED
                                              : ConnectionState::CONNECTING);
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

void FlightStorageClass::callDataChangeCallbacksTaskEntry(void* args) {
    getInstancePtr()->callDataChangeCallbacks(*static_cast<DataUpdateType*>(args));
    vTaskDelete(nullptr);
}

void FlightStorageClass::callbackLoopEntry(void* param) {
    auto* instance = static_cast<FlightStorageClass*>(param);
    instance->callbackLoop();
}

[[noreturn]] void FlightStorageClass::callbackLoop() {
    DataUpdateType updateType;
    while (true) {
        if (xQueueReceive(dataUpdateQueue, &updateType, portMAX_DELAY) == pdTRUE) {
            //xTaskCreate(callbackLoopEntry, "FlightStorageCallbackLoop", 8192, &updateType, 10, nullptr);
            callDataChangeCallbacks(updateType);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

#define FLIGHT_VAR(name, Name, type, initialValue, updateType, additionalCalls)\
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
        if (this->name == value)\
            return value;\
        [[maybe_unused]] auto originalValue = this->name;\
        this->name = value;\
        this->last##Name##UpdateTime = lastUpdateTime;\
        auto updateTypeVar = DataUpdateType::updateType;\
        additionalCalls\
        xQueueSendToBack(dataUpdateQueue, &updateTypeVar, pdMS_TO_TICKS(2));\
        return value;\
    }
#include "FlightVariables.inc"
#undef FLIGHT_VAR

void FlightStorageClass::addPointToFlightRoute(const Coordinate& point) {
    flightRoute.push_back(point);
    auto updateTypeVar = DataUpdateType::HISTORY;
    xQueueSendToBack(dataUpdateQueue, &updateTypeVar, portMAX_DELAY);
}
