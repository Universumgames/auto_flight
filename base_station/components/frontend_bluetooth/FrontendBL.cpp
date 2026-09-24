#include "FrontendBl.hpp"
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
    bluetoothManager.addDataWriteCallback(BLETopics::NotifyByte::BLE_TOPIC_AREA_DEFINE,
                                          [](const uint8_t* data, int len, BluetoothManager::TopicType topic) {
                                              try {
                                                  auto packet = nlohmann::json::from_cbor(data, data + len).get<
                                                      Frontend::AreaDefinePacket>();
                                                  FlightStorage.updatePlannedArea(packet.sourceId, AreaData{packet.shape, packet.settings});
                                                  ESP_LOGI(TAG_FRONTEND_BL, "Received new planned area with %d points",
                                                           packet.shape.size());
                                              }
                                              catch (const std::exception& e) {
                                                  ESP_LOGE(TAG_FRONTEND_BL, "Failed to parse AreaDefinePacket CBOR: %s",
                                                           e.what());
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

// Designated initializers can't name sourceId since it's inherited from BaseUpdatePacket rather
// than a direct member, so it's set below via plain assignment; suppress the resulting
// "missing initializer" noise for this block only.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

Frontend::PositionUpdatePacket FrontendHandlerBlClass::buildPositionUpdatePacket(uint32_t sourceId) {
    Frontend::PositionUpdatePacket packet{
        .position = FlightStorage.getPosition(sourceId),
        .positionUpdateTime = FlightStorage.getLastPositionUpdateTime(sourceId)
    };
    packet.sourceId = sourceId;
    return packet;
}

Frontend::ConnectionUpdatePacket FrontendHandlerBlClass::buildConnectionUpdatePacket(uint32_t sourceId) {
    Frontend::ConnectionUpdatePacket packet{
        .gpsConnection = FlightStorage.getGPSConnectionState(sourceId),
        .barometer = FlightStorage.getBarometerConnectionState(sourceId),
        .motorCom = FlightStorage.getMotorControlConnectionState(sourceId),
        .magnetometer = FlightStorage.getMagnetometerConnectionState(sourceId),
        .accelerometer = FlightStorage.getAccelerometerConnectionState(sourceId),
        .manualOverride = FlightStorage.getManualOverride(sourceId),
        .flightState = FlightStorage.getFlightState(sourceId)
    };
    packet.sourceId = sourceId;
    return packet;
}

Frontend::SensorPacket FrontendHandlerBlClass::buildSensorPacket(uint32_t sourceId) {
    Frontend::SensorPacket packet{
        .barometerPressure = FlightStorage.getPressure(sourceId),
        .calculatedAltitude = BarometerClass::calculateAltitude(FlightStorage.getBasePressure(),
                                                          FlightStorage.getPressure(sourceId)),
        .heading = FlightStorage.getHeading(sourceId)
    };
    packet.sourceId = sourceId;
    return packet;
}

Frontend::BatteryStatusPacket FrontendHandlerBlClass::buildBatteryStatusPacket(uint32_t sourceId) {
    Frontend::BatteryStatusPacket packet{
        .batteryPercentage = FlightStorage.getBatteryPercentage(sourceId)
    };
    packet.sourceId = sourceId;
    return packet;
}

Frontend::PlannedRoutePacket FrontendHandlerBlClass::buildPlannedRoutePacket(uint32_t sourceId) {
    Frontend::PlannedRoutePacket packet{
        .route = FlightStorage.getPlannedRoute(sourceId).getRoutePoints()
    };
    packet.sourceId = sourceId;
    return packet;
}

Frontend::AreaDefinePacket FrontendHandlerBlClass::buildAreaDefinePacket(uint32_t sourceId) {
    Frontend::AreaDefinePacket packet{
        .shape = FlightStorage.getPlannedArea(sourceId).getAreaPoints(),
        .settings = FlightStorage.getPlannedArea(sourceId).getSettings()
    };
    packet.sourceId = sourceId;
    return packet;
}

#pragma GCC diagnostic pop

void FrontendHandlerBlClass::sendUpdate(const BLETopics::NotifyByte topic, const nlohmann::json& packet) {
    const std::vector<uint8_t> cborData = nlohmann::json::to_cbor(packet);
    bluetoothManager.notify(topic, const_cast<uint8_t*>(cborData.data()), static_cast<int>(cborData.size()));
}

void FrontendHandlerBlClass::sendUpdate(BLETopics::NotifyByte topic, const std::function<nlohmann::json(uint32_t)>& packetMethod) {
    for (const auto& [planeId, info] : FlightStorage.getAllPlanes()) {
        sendUpdate(topic, packetMethod(planeId));
    }
    sendUpdate(topic, packetMethod(FlightStorageClass::BASE_ID));
}

void FrontendHandlerBlClass::sendUpdate(const BLETopics::NotifyByte topic) {
    switch (topic) {
    case BLETopics::BLE_TOPIC_POSITION_UPDATE:
        sendUpdate(topic, &FrontendHandlerBlClass::buildPositionUpdatePacket);
        break;
    case BLETopics::BLE_TOPIC_CONNECTION_UPDATE:
        sendUpdate(topic, &FrontendHandlerBlClass::buildConnectionUpdatePacket);
        break;
    case BLETopics::BLE_TOPIC_SENSOR_DATA:
        sendUpdate(topic, &FrontendHandlerBlClass::buildSensorPacket);
        break;
    case BLETopics::BLE_TOPIC_BATTERY_STATUS:
        sendUpdate(topic, &FrontendHandlerBlClass::buildBatteryStatusPacket);
        break;
    case BLETopics::BLE_TOPIC_PLANNED_ROUTE:
        sendUpdate(topic, &FrontendHandlerBlClass::buildPlannedRoutePacket);
        break;
    case BLETopics::BLE_TOPIC_AREA_DEFINE:
        sendUpdate(topic, &FrontendHandlerBlClass::buildAreaDefinePacket);
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

void FrontendHandlerBlClass::planeDataUpdateCallback(FlightStorageClass::DataUpdateType type, uint32_t sourceId) {
    auto packetType = static_cast<Frontend::PacketType>(static_cast<uint8_t>(type));
    switch (packetType) {
    case Frontend::PacketType::POSITION_UPDATE:
        sendUpdate(BLETopics::BLE_TOPIC_POSITION_UPDATE, buildPositionUpdatePacket(sourceId));
        break;
    case Frontend::PacketType::CONNECTION_UPDATE:
        sendUpdate(BLETopics::BLE_TOPIC_CONNECTION_UPDATE, buildConnectionUpdatePacket(sourceId));
        break;
    case Frontend::PacketType::SENSOR_UPDATE:
        sendUpdate(BLETopics::BLE_TOPIC_SENSOR_DATA, buildSensorPacket(sourceId));
        break;
    case Frontend::PacketType::BATTERY_STATUS:
        sendUpdate(BLETopics::BLE_TOPIC_BATTERY_STATUS, buildBatteryStatusPacket(sourceId));
        break;
    case Frontend::PacketType::PLANNED_ROUTE:
        sendUpdate(BLETopics::BLE_TOPIC_PLANNED_ROUTE, buildPlannedRoutePacket(sourceId));
        break;
    case Frontend::PacketType::AREA_DEFINE:
        sendUpdate(BLETopics::BLE_TOPIC_AREA_DEFINE, buildAreaDefinePacket(sourceId));
        break;
    default:
        // BLE_TOPIC_ALL (or anything unrecognized) has no packet of its own.
        break;
    }
}
