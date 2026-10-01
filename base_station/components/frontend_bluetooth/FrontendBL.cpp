#include "FrontendBl.hpp"
#include "esp_log.h"
#include "FlightStorage.hpp"
#include "Barometer.hpp"

#include <algorithm>

#include "Cache.hpp"

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
                                          [](const uint8_t* data, int len, BluetoothManager::TopicType topic) {
                                              try {
                                                  auto basePacket = BasePacket(data, static_cast<size_t>(len));
                                                  auto packet = std::make_unique<uint8_t[]>(len);
                                                  std::memcpy(packet.get(), data, len);
                                                  Cache.savePacket(basePacket.id, basePacket.type, {
                                                                       std::move(packet), static_cast<size_t>(len)
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
    for (const auto& planeId : FlightStorage.getAllPlanes() | std::views::keys) {
        auto [packet, len] = Cache.getLatestPacket(planeId, static_cast<PacketType>(topic));
        if (packet != nullptr) {
            sendRawData(static_cast<PacketType>(topic), packet, len);
        }
        else {
            ESP_LOGW(TAG_FRONTEND_BL, "No cached packet found for planeId=%u and topic=0x%02x", planeId,
                     static_cast<int>(topic));
            auto empty = BasePacket::nullPacket(planeId, (PacketType)topic);
            auto serializedPacket = empty.serialize();
            sendRawData(static_cast<PacketType>(topic), serializedPacket.first.get(), serializedPacket.second);
        }
    }
}

void FrontendHandlerBlClass::requestOutOfCycleUpdate(const BLETopics::NotifyByte topic) {
    if (refreshRequestQueue == nullptr) return; // Not initialized yet.
    // Non-blocking: this runs on the NimBLE host task (see BluetoothManager::onSVCGattHandler),
    // which must return promptly. If the queue is ever full, the request is simply dropped -
    // the topic still gets its next periodic update within a second regardless.
    xQueueSend(refreshRequestQueue, &topic, 0);
}

void FrontendHandlerBlClass::sendUpdateQueueTaskEntry(void* param) {
    auto* instance = instanceBL;
    while (true) {
        BLETopics::NotifyByte requestedTopic;
        if (xQueueReceive(instance->refreshRequestQueue, &requestedTopic, pdMS_TO_TICKS(2000))) {
            instance->sendAllSourcesData(requestedTopic);
        }
    }
}

void FrontendHandlerBlClass::triggerPeriodicUpdateTaskEntry(void* param) {
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

void FrontendHandlerBlClass::planeDataUpdateCallback(uint32_t sourceId, const PacketType type,
                                                     const RawSerializedPacket data, const size_t len) {
    sendRawData(type, data, len);
}

void FrontendHandlerBlClass::sendRawData(PacketType type, const RawSerializedPacket data, const size_t len) {
    WITH_RECURSIVE_MUTEX(bluetoothMutex) {
        bluetoothManager.notify(static_cast<uint8_t>(type), const_cast<uint8_t*>(data), len);
    }
}
