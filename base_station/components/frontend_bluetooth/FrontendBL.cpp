#include "./FrontendBl.hpp"
#include "esp_log.h"
#include "FlightStorage.hpp"
#include "Barometer.hpp"

#include <algorithm>

namespace {
    /// The base station may know about several planes; until there's real fleet-selection UI,
    /// report on whichever plane we actually have data for.
    uint32_t currentPlaneId() {
        const auto& planes = FlightStorage.getAllPlanes();
        return planes.empty() ? FlightStorageClass::NO_PLANE_ID : planes.begin()->first;
    }
}

const char* FrontendHandlerBlClass::TAG_FRONTEND_BL = "FrontendBL";

static FrontendHandlerBlClass* instanceBL = nullptr;

FrontendHandlerBlClass& FrontendHandlerBl = FrontendHandlerBlClass::getInstance();

FrontendHandlerBlClass* FrontendHandlerBlClass::getInstancePtr() {
    if (instanceBL == nullptr) {
        instanceBL = new FrontendHandlerBlClass();
    }
    return instanceBL;
}

FrontendHandlerBlClass& FrontendHandlerBlClass::getInstance() {
    return *getInstancePtr();
}

void FrontendHandlerBlClass::init() {
    std::vector<BluetoothManager::Characteristic> characteristics;
    for (const auto i : BLETopics::ALL_NOTIFICATIONS) {
        characteristics.push_back(BluetoothManager::Characteristic{
            .topicID = static_cast<BluetoothManager::TopicType>(i),
            .uuid = BLETopics::getUUIDForTopic(i),
            .val_handle = 0
        });
    }
    bluetoothManager.init(characteristics);

    refreshRequestQueue = xQueueCreate(BLETopics::ALL_NOTIFICATIONS_SIZE, sizeof(BLETopics::NotifyByte));

    registerReadTriggerCallback();

    xTaskCreate(
        sendUpdateTaskEntry,
        "bl_update_task",
        4096,
        nullptr,
        tskIDLE_PRIORITY + 1,
        nullptr
    );
    xTaskCreate(
        sendAutomaticUpdateTaskEntry,
        "bl_auto_update_task",
        4096,
        nullptr,
        tskIDLE_PRIORITY + 1,
        nullptr
    );
    ESP_LOGI(TAG_FRONTEND_BL, "Frontend handler initialized");
}

void FrontendHandlerBlClass::registerReadTriggerCallback() {
    bluetoothManager.addDataWriteCallback(BLETopics::NotifyByte::BLE_TOPIC_AREA_DEFINE, [](const uint8_t* data, int len, BluetoothManager::TopicType topic) {
        try {
            auto packet = nlohmann::json::from_cbor(data, data + len).get<Frontend::AreaDefinePacket>();
            FlightStorage.updatePlannedArea(packet.planeId, packet.shape);
            ESP_LOGI(TAG_FRONTEND_BL, "Received new planned area with %d points", packet.shape.size());
        } catch (const std::exception& e) {
            ESP_LOGE(TAG_FRONTEND_BL, "Failed to parse AreaDefinePacket CBOR: %s", e.what());
        }
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered write callback for BLE_TOPIC_AREA_DEFINE");

    // Every topic is read-triggerable: a plain GATT read carries no data (see
    // BluetoothManager::onSVCGattHandler) and instead just asks for a fresh
    // notification on that characteristic. One callback handles all of them.
    bluetoothManager.addReadTriggerCallback([this](const BluetoothManager::TopicType topic) {
        requestOutOfCycleUpdate(static_cast<BLETopics::NotifyByte>(topic));
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered read-trigger callback for all topics");
}

Frontend::FlightUpdatePacket FrontendHandlerBlClass::buildFlightUpdatePacket() {
    const uint32_t planeId = currentPlaneId();
    Frontend::FlightUpdatePacket packet{
        .basePosition = FlightStorage.getBasePosition(),
        .basePositionUpdateTime = FlightStorage.getLastBasePositionUpdateTime(),
        .planePosition = FlightStorage.getPlanePosition(planeId),
        .planePositionUpdateTime = FlightStorage.getLastPlanePositionUpdateTime(planeId),
        .flightRoute = FlightStorage.getFlightRoute(planeId),
        .flightRouteUpdateTime = FlightStorage.getLastFlightRouteUpdateTime(planeId),
        .plannedRoute = FlightStorage.getPlannedRoute(planeId),
        .plannedRouteUpdateTime = FlightStorage.getLastPlannedRouteUpdateTime(planeId)
    };
    packet.planeId = planeId;
    return packet;
}

Frontend::ConnectionUpdatePacket FrontendHandlerBlClass::buildConnectionUpdatePacket() {
    const uint32_t planeId = currentPlaneId();
    Frontend::ConnectionUpdatePacket packet{
        .baseConnectionState = FlightStorage.getBaseConnectionState(),
        .lastContactBaseStationTimestamp = FlightStorage.getLastBaseConnectionStateUpdateTime(),
        .planeConnectionState = FlightStorage.getPlaneConnectionState(planeId),
        .lastContactPlaneTimestamp = FlightStorage.getLastPlaneConnectionStateUpdateTime(planeId),
        .gpsConnectionBase = FlightStorage.getBaseGPSConnectionState(),
        .gpsConnectionPlane = FlightStorage.getPlaneGPSConnectionState(planeId),
        .barometerConnectionBase = FlightStorage.getBaseBarometerConnectionState(),
        .barometerConnectionPlane = FlightStorage.getPlaneBarometerConnectionState(planeId),
        .motorComConnectionPlane = FlightStorage.getPlaneMotorControlConnectionState(planeId),
        .magnetometerConnectionPlane = FlightStorage.getPlaneMagnetometerConnectionState(planeId),
        .accelerometerConnectionPlane = FlightStorage.getPlaneAccelerometerConnectionState(planeId),
        .manualOverridePlane = FlightStorage.getPlaneManualOverride(planeId),
        .flightState = FlightStorage.getFlightState(planeId)
    };
    packet.planeId = planeId;
    return packet;
}

Frontend::SensorPacket FrontendHandlerBlClass::buildSensorPacket() {
    const uint32_t planeId = currentPlaneId();
    Frontend::SensorPacket packet{
        .barometerPressureBase = FlightStorage.getBasePressure(),
        .barometerPressurePlane = FlightStorage.getPlanePressure(planeId),
        .calculatedAltitude = Barometer.calculateAltitude(FlightStorage.getBasePressure(), FlightStorage.getPlanePressure(planeId)),
        .headingPlane = FlightStorage.getPlaneHeading(planeId)
    };
    packet.planeId = planeId;
    return packet;
}

Frontend::BatteryStatusPacket FrontendHandlerBlClass::buildBatteryStatusPacket() {
    const uint32_t planeId = currentPlaneId();
    Frontend::BatteryStatusPacket packet{
        .baseBatteryPercentage = FlightStorage.getBaseBatteryPercentage(),
        .planeBatteryPercentage = FlightStorage.getPlaneBatteryPercentage(planeId)
    };
    packet.planeId = planeId;
    return packet;
}

Frontend::PlannedRoutePacket FrontendHandlerBlClass::buildPlannedRoutePacket() {
    const uint32_t planeId = currentPlaneId();
    Frontend::PlannedRoutePacket packet{
        .route = FlightStorage.getPlannedRoute(planeId)
    };
    packet.planeId = planeId;
    return packet;
}

Frontend::AreaDefinePacket FrontendHandlerBlClass::buildAreaDefinePacket() {
    const uint32_t planeId = currentPlaneId();
    Frontend::AreaDefinePacket packet{
        .shape = FlightStorage.getPlannedArea(planeId)
    };
    packet.planeId = planeId;
    return packet;
}

void FrontendHandlerBlClass::sendUpdate(const BLETopics::NotifyByte topic, const nlohmann::json& packet) {
    const std::vector<uint8_t> cborData = nlohmann::json::to_cbor(packet);
    bluetoothManager.notify(topic, const_cast<uint8_t*>(cborData.data()), static_cast<int>(cborData.size()));
}

void FrontendHandlerBlClass::sendUpdate(const BLETopics::NotifyByte topic) {
    switch (topic) {
        case BLETopics::BLE_TOPIC_FLIGHT_UPDATE:
            sendUpdate(topic, buildFlightUpdatePacket());
            break;
        case BLETopics::BLE_TOPIC_CONNECTION_UPDATE:
            sendUpdate(topic, buildConnectionUpdatePacket());
            break;
        case BLETopics::BLE_TOPIC_SENSOR_DATA:
            sendUpdate(topic, buildSensorPacket());
            break;
        case BLETopics::BLE_TOPIC_BATTERY_STATUS:
            sendUpdate(topic, buildBatteryStatusPacket());
            break;
        case BLETopics::BLE_TOPIC_PLANNED_ROUTE:
            sendUpdate(topic, buildPlannedRoutePacket());
            break;
        case BLETopics::BLE_TOPIC_AREA_DEFINE:
            sendUpdate(topic, buildAreaDefinePacket());
            break;
        default:
            // BLE_TOPIC_ALL (or anything unrecognized) has no packet of its own.
            break;
    }
}

void FrontendHandlerBlClass::requestOutOfCycleUpdate(const BLETopics::NotifyByte topic) {
    if (refreshRequestQueue == nullptr) return; // Not initialized yet.
    // Non-blocking: this runs on the NimBLE host task (see BluetoothManager::onSVCGattHandler),
    // which must return promptly. If the queue is ever full, the request is simply dropped -
    // the topic still gets its next periodic update within a second regardless.
    xQueueSend(refreshRequestQueue, &topic, 0);
}

void FrontendHandlerBlClass::sendUpdateTaskEntry(void* param) {
    auto* instance = instanceBL;
    while (true) {
        BLETopics::NotifyByte requestedTopic;
        if (xQueueReceive(instance->refreshRequestQueue, &requestedTopic, pdMS_TO_TICKS(200))) {
            instance->sendUpdate(requestedTopic);
        }
    }
}

void FrontendHandlerBlClass::sendAutomaticUpdateTaskEntry(void* param) {
    auto* instance = instanceBL;
    while (true) {
        for (const auto topic : BLETopics::ALL_NOTIFICATIONS) {
            instance->requestOutOfCycleUpdate(topic);
        }
        // Also publish the base station's own battery via the standard BLE Battery Service,
        // for generic BLE clients that don't know this project's custom topics.
        const int baseBatteryPercentage = std::max(0, std::min(100, FlightStorage.getBaseBatteryPercentage()));
        instance->bluetoothManager.notifyBatteryLevel(static_cast<uint8_t>(baseBatteryPercentage));
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}
