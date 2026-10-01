#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "packets/base.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

class CacheClass {
private:
    CacheClass() = default;
public:
    ~CacheClass() = delete;
    CacheClass(const CacheClass &) = delete;

    static CacheClass* getInstancePtr();
    static CacheClass& getInstance();

private:
    struct PacketCacheEntry {
        RawSerializedPacket data;
        size_t len;
    };

    std::unordered_map<uint32_t, std::unordered_map<PacketType, PacketCacheEntry>> cache;

    std::vector<std::function<void(uint32_t, PacketType, RawSerializedPacket, size_t)>> packetCallbacks;

    struct PacketCallbackEvent {
        uint32_t sourceId;
        PacketType type;
    };

    QueueHandle_t packetCallbackQueue = nullptr;
    // recursive, so callbacks invoked while holding it can still use the cache
    SemaphoreHandle_t cacheMutex = nullptr;

    void invokePacketCallback(uint32_t sourceId, PacketType type);

    void queuePacketCallback(uint32_t sourceId, PacketType type);

    static void callbackLoopEntry(void* param);

    [[noreturn]] void callbackLoop();

public:
    void init();

    /**
     * @brief Save a packet in the cache.
     * @param sourceId The ID of the source of the packet.
     * @param type The type of the packet.
     * @param data The raw serialized packet data, owned by the called.
     * @param len The length of the packet data.
     */
    void savePacket(uint32_t sourceId, PacketType type, RawSerializedPacket data, size_t len);

    /**
     * @brief Save a packet in the cache.
     * @param sourceId The ID of the source of the packet.
     * @param type The type of the packet.
     * @param data The serialized packet data, owned by the called.
     */
    void savePacket(uint32_t sourceId, PacketType type, SerializedPacket data);

    bool hasPacket(uint32_t sourceId, PacketType type);

    std::pair<RawSerializedPacket, size_t> getLatestPacket(uint32_t sourceId, PacketType type);

    void registerPacketCallback(const std::function<void(uint32_t, PacketType, RawSerializedPacket, size_t)>& callback);
};

extern CacheClass& Cache;