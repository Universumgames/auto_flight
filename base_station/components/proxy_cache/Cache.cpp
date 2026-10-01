#include "Cache.hpp"

#include "esp_log.h"

#define WITH_RECURSIVE_MUTEX(mutex) \
for (bool _once = (xSemaphoreTakeRecursive((mutex), portMAX_DELAY) == pdTRUE); \
_once; \
_once = false, xSemaphoreGiveRecursive((mutex)))

static CacheClass* instancePtr = nullptr;
CacheClass& Cache = CacheClass::getInstance();

CacheClass* CacheClass::getInstancePtr() {
    if (instancePtr == nullptr) {
        instancePtr = new CacheClass();
    }
    return instancePtr;
}

CacheClass& CacheClass::getInstance() {
    return *getInstancePtr();
}

void CacheClass::init() {
    cacheMutex = xSemaphoreCreateRecursiveMutex();
    if (cacheMutex == nullptr) {
        ESP_LOGE("Cache", "Failed to create cache mutex");
        return;
    }

    packetCallbackQueue = xQueueCreate(50, sizeof(PacketCallbackEvent));
    if (packetCallbackQueue == nullptr) {
        ESP_LOGE("Cache", "Failed to create packet callback queue");
        return;
    }

    xTaskCreate(callbackLoopEntry, "CacheCallbackLoop", 4096, this, tskIDLE_PRIORITY + 1, nullptr);
}

void CacheClass::savePacket(const uint32_t sourceId, const PacketType type, const RawSerializedPacket data, const size_t len) {
    WITH_RECURSIVE_MUTEX(cacheMutex) {
        if (hasPacket(sourceId, type)) {
            // Free the previously stored data to avoid memory leaks
            delete[] cache[sourceId][type].data;
        }
        cache[sourceId][type] = {.data = data, .len = len};
    }
    queuePacketCallback(sourceId, type);
}

void CacheClass::savePacket(const uint32_t sourceId, const PacketType type, SerializedPacket data) {
    WITH_RECURSIVE_MUTEX(cacheMutex) {
        if (hasPacket(sourceId, type)) {
            // Free the previously stored data to avoid memory leaks
            delete[] cache[sourceId][type].data;
        }
        cache[sourceId][type] = {.data = data.first.release(), .len = data.second};
    }
    queuePacketCallback(sourceId, type);
}

std::pair<RawSerializedPacket, size_t> CacheClass::getLatestPacket(uint32_t sourceId, PacketType type) {
    std::pair<RawSerializedPacket, size_t> result = {nullptr, 0};
    WITH_RECURSIVE_MUTEX(cacheMutex) {
        if (hasPacket(sourceId, type)) {
            const auto& entry = cache[sourceId][type];
            result = {entry.data, entry.len};
        }
    }
    return result;
}

bool CacheClass::hasPacket(uint32_t sourceId, PacketType type) {
    bool found = false;
    WITH_RECURSIVE_MUTEX(cacheMutex) {
        const auto it = cache.find(sourceId);
        found = it != cache.end() && it->second.find(type) != it->second.end();
    }
    return found;
}

void CacheClass::invokePacketCallback(uint32_t sourceId, PacketType type) {
    // hold the lock during the callbacks, so the data can't be freed by a concurrent savePacket
    WITH_RECURSIVE_MUTEX(cacheMutex) {
        auto [data, len] = getLatestPacket(sourceId, type);
        if (data != nullptr) {
            for (const auto& callback : packetCallbacks) {
                callback(sourceId, type, data, len);
            }
        }
    }
}

void CacheClass::queuePacketCallback(uint32_t sourceId, PacketType type) {
    if (packetCallbackQueue == nullptr) {
        return;
    }
    PacketCallbackEvent event{sourceId, type};
    if (xQueueSendToBack(packetCallbackQueue, &event, 0) != pdTRUE) {
        ESP_LOGW("Cache", "Packet callback queue full, dropping callback for source %lu", static_cast<unsigned long>(sourceId));
    }
}

void CacheClass::callbackLoopEntry(void* param) {
    auto* instance = static_cast<CacheClass*>(param);
    instance->callbackLoop();
}

[[noreturn]] void CacheClass::callbackLoop() {
    PacketCallbackEvent event{};
    while (true) {
        if (xQueueReceive(packetCallbackQueue, &event, portMAX_DELAY) == pdTRUE) {
            invokePacketCallback(event.sourceId, event.type);
        }
    }
}

void CacheClass::registerPacketCallback(const std::function<void(uint32_t, PacketType, RawSerializedPacket, size_t)>& callback) {
    packetCallbacks.push_back(callback);
}
