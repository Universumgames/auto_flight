#include "FrontendBl.hpp"
#include "esp_log.h"
#include "Barometer.hpp"

#include <algorithm>

#include "Battery.hpp"
#include "Cache.hpp"
#include "GPS_Reader.hpp"

#define WITH_RECURSIVE_MUTEX(mutex) \
for (bool _once = (xSemaphoreTakeRecursive((mutex), portMAX_DELAY) == pdTRUE); \
_once; \
_once = false, xSemaphoreGiveRecursive((mutex)))

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
    bluetoothMutex = xSemaphoreCreateRecursiveMutex();
    if (bluetoothMutex == nullptr) {
        ESP_LOGE("Cache", "Failed to create cache mutex");
        return;
    }

    std::vector<BluetoothManager::Characteristic> characteristics;
    for (const auto i : BLETopics::ALL_NOTIFICATIONS) {
        characteristics.push_back(BluetoothManager::Characteristic{
            .topicID = static_cast<BluetoothManager::TopicType>(i),
            .uuid = BLETopics::getUUIDForTopic(i),
            .val_handle = 0
        });
    }
    bluetoothManager.init(characteristics);

    for (const auto i : BLETopics::ALL_NOTIFICATIONS) {
        topicUpdateIntervals[i] = 0;
    }

    refreshRequestQueue = xQueueCreate(BLETopics::ALL_NOTIFICATIONS_SIZE, sizeof(BLETopics::NotifyByte));

    registerReadTriggerCallback();
    Cache.registerPacketCallback([this](uint32_t sourceId, PacketType type, RawSerializedPacket data, size_t len) {
        planeDataUpdateCallback(sourceId, type, data, len);
    });

    xTaskCreate(
        sendUpdateQueueTaskEntry,
        "bl_update_task",
        4096,
        nullptr,
        tskIDLE_PRIORITY + 1,
        nullptr
    );
    xTaskCreate(
        triggerPeriodicUpdateTaskEntry,
        "bl_auto_update_task",
        4096,
        nullptr,
        tskIDLE_PRIORITY + 1,
        nullptr
    );
    ESP_LOGI(TAG_FRONTEND_BL, "Frontend handler initialized");
}

void FrontendHandlerBlClass::registerReadTriggerCallback() {
    bluetoothManager.addDataWriteCallback(BLETopics::NotifyByte::BLE_PLANNED_AREA,
                                          [](const uint8_t* data, size_t len, BluetoothManager::TopicType topic) {
                                              try {
                                                  const auto basePacket = BasePacket(data, len);
                                                  auto packet = std::make_unique<uint8_t[]>(len);
                                                  std::memcpy(packet.get(), data, len);
                                                  Cache.savePacket(basePacket.id, basePacket.type, {
                                                                       std::move(packet), len
                                                                   });
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

void FrontendHandlerBlClass::sendAllSourcesData(const BLETopics::NotifyByte topic) {
    ESP_LOGI(TAG_FRONTEND_BL, "Sending data for topic=0x%02x from all planes", topic);
    for (const auto& planeId : Cache.getSources()) {
        ESP_LOGI(TAG_FRONTEND_BL, "Sending data for planeId=%" PRIu32 " and topic=0x%02x", planeId, topic);
        auto packetType = BLETopics::toPacketType(topic).value();
        // copy, as the cached buffer may be freed by a concurrent savePacket while it is being sent
        auto [packet, len] = Cache.copyLatestPacket(planeId, packetType);
        if (packet != nullptr) {
            sendRawData(packetType, packet.get(), len);
        }
        else {
            ESP_LOGW(TAG_FRONTEND_BL, "No cached packet found for planeId=%" PRIu32 " and topic=0x%02x", planeId,
                     topic);
            auto empty = BasePacket::nullPacket(planeId, packetType, GPS_Reader.getGPSLatestTime());
            auto serializedPacket = empty.serialize();
            sendRawData(packetType, serializedPacket.first.get(), serializedPacket.second);
            serializedPacket.first.reset();
        }
    }
}

void FrontendHandlerBlClass::requestOutOfCycleUpdate(const BLETopics::NotifyByte topic) {
    if (refreshRequestQueue == nullptr) return; // Not initialized yet.
    // Non-blocking: this runs on the NimBLE host task (see BluetoothManager::onSVCGattHandler),
    // which must return promptly. If the queue is ever full, the request is simply dropped -
    // the topic still gets its next periodic update within a second regardless.
    xQueueSend(refreshRequestQueue, &topic, pdMS_TO_TICKS(1));
    ESP_LOGI(TAG_FRONTEND_BL, "Enqueued out-of-cycle update request for topic 0x%02x", topic);
}

void FrontendHandlerBlClass::sendUpdateQueueTaskEntry(void* param) {
    instanceBL->sendUpdateQueueTask();
}

void FrontendHandlerBlClass::sendUpdateQueueTask() {
    while (true) {
        BLETopics::NotifyByte requestedTopic;
        if (xQueueReceive(refreshRequestQueue, &requestedTopic, pdMS_TO_TICKS(2000))) {
            sendAllSourcesData(requestedTopic);
        }
    }
}

void FrontendHandlerBlClass::triggerPeriodicUpdateTaskEntry(void* param) {
    instanceBL->triggerPeriodicUpdateTask();
}

void FrontendHandlerBlClass::triggerPeriodicUpdateTask() {
    while (true) {
        for (const auto topic : BLETopics::ALL_NOTIFICATIONS) {
            topicUpdateIntervals[topic]++;
            if (topicUpdateIntervals[topic] >= getUpdateIntervalForTopic(topic)) {
                //xQueueSend(refreshRequestQueue, &topic, pdMS_TO_TICKS(1));
                topicUpdateIntervals[topic] = 0;
            }
        }
        // Also publish the base station's own battery via the standard BLE Battery Service,
        // for generic BLE clients that don't know this project's custom topics.
        const uint8_t baseBatteryPercentage = std::max((uint8_t)0, std::min((uint8_t)100, Battery.getLastMeasuredVoltagePercentage()));
        bluetoothManager.notifyBatteryLevel(baseBatteryPercentage);
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

void FrontendHandlerBlClass::planeDataUpdateCallback(uint32_t sourceId, const PacketType type,
                                                     const RawSerializedPacket data, const size_t len) {
    sendRawData(type, data, len);
}

void FrontendHandlerBlClass::sendRawData(const PacketType type, const RawSerializedPacket data, const size_t len) {
    WITH_RECURSIVE_MUTEX(bluetoothMutex) {
        for (int i = 0; i < len; i++) {
            printf("%02x ", data[i]);
        }
        printf("\n");
        bluetoothManager.notify(BLETopics::toNotifyByte(type).value(), const_cast<uint8_t*>(data), len);
    }
}


int FrontendHandlerBlClass::getUpdateIntervalForTopic(const BLETopics::NotifyByte topic) {
    switch (topic) {
    case BLETopics::NotifyByte::BLE_SENSOR_UPDATE:
        return 1;
    case BLETopics::NotifyByte::BLE_POSITION:
        return 1;
    case BLETopics::NotifyByte::BLE_COMPONENT_STATUS:
        return 2;
    case BLETopics::NotifyByte::BLE_PLANNED_ROUTE:
        return 50;
    case BLETopics::NotifyByte::BLE_PLANNED_AREA:
        return 50;
    case BLETopics::NotifyByte::BLE_ROUTE_HISTORY:
        return 50;
    case BLETopics::NotifyByte::BLE_ROUTE_HISTORY_REQUEST:
        return 500;
    case BLETopics::NotifyByte::BLE_PLANNED_ROUTE_CONFIRMATION:
        return 500;
    }
    return 0;
}
