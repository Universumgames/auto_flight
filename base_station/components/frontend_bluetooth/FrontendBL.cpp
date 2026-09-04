#include "./FrontendBl.hpp"
#include "esp_log.h"
#include "FlightStorage.hpp"
#include "Barometer.hpp"

#include <algorithm>


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

#define PACKET_TO_JSON_RESPONSE(packet) \
    std::string jsonStr = nlohmann::json(packet).dump(); \
    *len = jsonStr.size(); \
    auto* data = new uint8_t[*len]; \
    memcpy(data, jsonStr.c_str(), *len); \
    return data;

void FrontendHandlerBlClass::registerReadCallbacks() {
    // AREA_DEFINE, FLIGHT_UPDATE and PLANNED_ROUTE can exceed the 512-byte GATT
    // attribute limit (BLE_ATT_ATTR_MAX_LEN) once routes/areas have many points, so
    // they're served exclusively over the fragmented notify path (see sendUpdate())
    // rather than as GATT reads, which have no equivalent of unbounded fragmentation.
    bluetoothManager.addDataWriteCallback(BLETopics::NotifyByte::BLE_TOPIC_AREA_DEFINE, [](const uint8_t* data, int len, BluetoothManager::TopicType topic) {
        std::string jsonStr(reinterpret_cast<const char*>(data), len);
        try {
            auto packet = nlohmann::json::parse(jsonStr).get<Frontend::AreaDefinePacket>();
            FlightStorage.updatePlannedArea(packet.shape);
            ESP_LOGI(TAG_FRONTEND_BL, "Received new planned area with %d points", packet.shape.size());
        } catch (const std::exception& e) {
            ESP_LOGE(TAG_FRONTEND_BL, "Failed to parse AreaDefinePacket JSON: %s", e.what());
        }
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered write callback for BLE_TOPIC_AREA_DEFINE");

    bluetoothManager.addDataReadCallback(BLETopics::NotifyByte::BLE_TOPIC_CONNECTION_UPDATE, [this](int* len, BluetoothManager::TopicType topic) -> uint8_t* {
        auto packet = buildConnectionUpdatePacket();
        PACKET_TO_JSON_RESPONSE(packet);
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered read callback for BLE_TOPIC_CONNECTION_UPDATE");

    bluetoothManager.addDataReadCallback(BLETopics::NotifyByte::BLE_TOPIC_SENSOR_DATA, [this](int* len, BluetoothManager::TopicType topic) -> uint8_t* {
        auto packet = buildSensorPacket();
        PACKET_TO_JSON_RESPONSE(packet);
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered read callback for BLE_TOPIC_SENSOR_DATA");

    bluetoothManager.addDataReadCallback(BLETopics::NotifyByte::BLE_TOPIC_BATTERY_STATUS, [this](int* len, BluetoothManager::TopicType topic) -> uint8_t* {
        auto packet = buildBatteryStatusPacket();
        PACKET_TO_JSON_RESPONSE(packet);
    });
    ESP_LOGI(TAG_FRONTEND_BL, "Registered read callback for BLE_TOPIC_BATTERY_STATUS");
}

Frontend::FlightUpdatePacket FrontendHandlerBlClass::buildFlightUpdatePacket() {
    return Frontend::FlightUpdatePacket{
        .basePosition = FlightStorage.getBasePosition(),
        .basePositionUpdateTime = FlightStorage.getLastBasePositionUpdateTime(),
        .planePosition = FlightStorage.getPlanePosition(),
        .planePositionUpdateTime = FlightStorage.getLastPlanePositionUpdateTime(),
        .flightRoute = FlightStorage.getFlightRoute(),
        .flightRouteUpdateTime = FlightStorage.getLastFlightRouteUpdateTime(),
        .plannedRoute = FlightStorage.getPlannedRoute(),
        .plannedRouteUpdateTime = FlightStorage.getLastPlannedRouteUpdateTime()
    };
}

Frontend::ConnectionUpdatePacket FrontendHandlerBlClass::buildConnectionUpdatePacket() {
    return Frontend::ConnectionUpdatePacket{
        .baseConnectionState = FlightStorage.getBaseConnectionState(),
        .lastContactBaseStationTimestamp = FlightStorage.getLastBaseConnectionStateUpdateTime(),
        .planeConnectionState = FlightStorage.getPlaneConnectionState(),
        .lastContactPlaneTimestamp = FlightStorage.getLastPlaneConnectionStateUpdateTime(),
        .gpsConnectionBase = FlightStorage.getBaseGPSConnectionState(),
        .gpsConnectionPlane = FlightStorage.getPlaneGPSConnectionState(),
        .barometerConnectionBase = FlightStorage.getBaseBarometerConnectionState(),
        .barometerConnectionPlane = FlightStorage.getPlaneBarometerConnectionState(),
        .motorComConnectionPlane = FlightStorage.getPlaneMotorControlConnectionState(),
        .magnetometerConnectionPlane = FlightStorage.getPlaneMagnetometerConnectionState(),
        .accelerometerConnectionPlane = FlightStorage.getPlaneAccelerometerConnectionState(),
        .manualOverridePlane = FlightStorage.getPlaneManualOverride(),
        .flightState = FlightStorage.getFlightState()
    };
}

Frontend::SensorPacket FrontendHandlerBlClass::buildSensorPacket() {
    return Frontend::SensorPacket{
        .barometerPressureBase = FlightStorage.getBasePressure(),
        .barometerPressurePlane = FlightStorage.getPlanePressure(),
        .calculatedAltitude = Barometer.calculateAltitude(FlightStorage.getBasePressure(), FlightStorage.getPlanePressure()),
        .headingPlane = FlightStorage.getPlaneHeading()
    };
}

Frontend::BatteryStatusPacket FrontendHandlerBlClass::buildBatteryStatusPacket() {
    return Frontend::BatteryStatusPacket{
        .baseBatteryPercentage = FlightStorage.getBaseBatteryPercentage(),
        .planeBatteryPercentage = FlightStorage.getPlaneBatteryPercentage()
    };
}

Frontend::PlannedRoutePacket FrontendHandlerBlClass::buildPlannedRoutePacket() {
    return Frontend::PlannedRoutePacket{
        .route = FlightStorage.getPlannedRoute()
    };
}

Frontend::AreaDefinePacket FrontendHandlerBlClass::buildAreaDefinePacket() {
    return Frontend::AreaDefinePacket{
        .shape = FlightStorage.getPlannedArea()
    };
}

void FrontendHandlerBlClass::sendUpdate(const BLETopics::NotifyByte topic, const nlohmann::json& packet) {
    const std::string jsonStr = packet.dump();
    bluetoothManager.notify(topic, reinterpret_cast<uint8_t*>(const_cast<char*>(jsonStr.data())), static_cast<int>(jsonStr.size()));
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
