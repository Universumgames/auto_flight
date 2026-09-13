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

    registerReadCallbacks();

    xTaskCreate(
        sendUpdateTaskEntry,
        "ws_update_task",
        4096,
        nullptr,
        tskIDLE_PRIORITY + 1,
        nullptr
    );
    ESP_LOGI(TAG_FRONTEND_BL, "Frontend handler initialized");
}

#define PACKET_TO_CBOR_RESPONSE(packet) \
    std::vector<uint8_t> cborData = nlohmann::json::to_cbor(nlohmann::json(packet)); \
    *len = static_cast<int>(cborData.size()); \
    auto* data = new uint8_t[*len]; \
    memcpy(data, cborData.data(), *len); \
    return data;

void FrontendHandlerBlClass::registerReadCallbacks() {
    // AREA_DEFINE, FLIGHT_UPDATE and PLANNED_ROUTE can exceed the 512-byte GATT
    // attribute limit (BLE_ATT_ATTR_MAX_LEN) once routes/areas have many points, so
    // they're served exclusively over the fragmented notify path (see sendUpdate())
    // rather than as GATT reads, which have no equivalent of unbounded fragmentation.
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

    bluetoothManager.addDataReadCallback(BLETopics::NotifyByte::BLE_TOPIC_CONNECTION_UPDATE, [this](int* len, BluetoothManager::TopicType topic) -> uint8_t* {
        auto packet = buildConnectionUpdatePacket();
        PACKET_TO_CBOR_RESPONSE(packet);
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered read callback for BLE_TOPIC_CONNECTION_UPDATE");

    bluetoothManager.addDataReadCallback(BLETopics::NotifyByte::BLE_TOPIC_SENSOR_DATA, [this](int* len, BluetoothManager::TopicType topic) -> uint8_t* {
        auto packet = buildSensorPacket();
        PACKET_TO_CBOR_RESPONSE(packet);
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered read callback for BLE_TOPIC_SENSOR_DATA");

    bluetoothManager.addDataReadCallback(BLETopics::NotifyByte::BLE_TOPIC_BATTERY_STATUS, [this](int* len, BluetoothManager::TopicType topic) -> uint8_t* {
        auto packet = buildBatteryStatusPacket();
        PACKET_TO_CBOR_RESPONSE(packet);
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered read callback for BLE_TOPIC_BATTERY_STATUS");
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

void FrontendHandlerBlClass::sendUpdate() {
    sendUpdate(BLETopics::BLE_TOPIC_FLIGHT_UPDATE, buildFlightUpdatePacket());
    sendUpdate(BLETopics::BLE_TOPIC_CONNECTION_UPDATE, buildConnectionUpdatePacket());
    sendUpdate(BLETopics::BLE_TOPIC_SENSOR_DATA, buildSensorPacket());
    sendUpdate(BLETopics::BLE_TOPIC_BATTERY_STATUS, buildBatteryStatusPacket());
    sendUpdate(BLETopics::BLE_TOPIC_PLANNED_ROUTE, buildPlannedRoutePacket());
    sendUpdate(BLETopics::BLE_TOPIC_AREA_DEFINE, buildAreaDefinePacket());

    // Also publish the base station's own battery via the standard BLE Battery Service,
    // for generic BLE clients that don't know this project's custom topics.
    const int baseBatteryPercentage = std::max(0, std::min(100, FlightStorage.getBaseBatteryPercentage()));
    bluetoothManager.notifyBatteryLevel(static_cast<uint8_t>(baseBatteryPercentage));
}

void FrontendHandlerBlClass::sendUpdateTaskEntry(void* param) {
    auto* instance = instanceBL;
    while (true) {
        instance->sendUpdate();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Send updates every second
    }
}
