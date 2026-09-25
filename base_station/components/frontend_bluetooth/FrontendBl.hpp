#pragma once
#include <cstdint>
#include <cstddef>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "BluetoothManager.hpp"
#include "FlightStorage.hpp"

class FrontendHandlerBlClass {
private:
    FrontendHandlerBlClass() = default;
    static const char* TAG_FRONTEND_BL;

    const char* local_device_name = CONFIG_BLUETOOTH_DEVICE_NAME;

public:
    ~FrontendHandlerBlClass() = delete;

    static FrontendHandlerBlClass* getInstancePtr();
    static FrontendHandlerBlClass& getInstance();

private:
    BluetoothManager bluetoothManager;
    /* Read-triggered refresh requests (requestOutOfCycleUpdate), one topic per entry.
     * sendUpdateTaskEntry drains this instead of waiting out the full periodic 1s tick.
     * Created once in init(); sized to one slot per topic, so a burst of reads across
     * every characteristic still queues without dropping. */
    QueueHandle_t refreshRequestQueue = nullptr;
public:
    void init();

private:

    void registerReadTriggerCallback();

    void registerInternalDataChangeCallbacks();

    void planeDataUpdateCallback(FlightStorageClass::DataUpdateType type, uint32_t sourceId);

    Frontend::PositionUpdatePacket buildPositionUpdatePacket(uint32_t sourceId);
    Frontend::ConnectionUpdatePacket buildConnectionUpdatePacket(uint32_t sourceId);
    Frontend::SensorPacket buildSensorPacket(uint32_t sourceId);
    Frontend::BatteryStatusPacket buildBatteryStatusPacket(uint32_t sourceId);
    Frontend::PlannedRoutePacket buildPlannedRoutePacket(uint32_t sourceId);
    Frontend::AreaDefinePacket buildAreaDefinePacket(uint32_t sourceId);

    /**
     * Sends a packet to the frontend over Bluetooth for the given topic
     * @param topic the topic to send the packet for
     * @param packet the packet to send
     */
    void sendUpdate(BLETopics::NotifyByte topic, const nlohmann::json& packet);

    /**
     * Builds and sends packets for every sourceId to the frontend over Bluetooth for the given topic
     * @param topic the topic to send the packet for
     * @param packetMethod a function that builds the packet for the given sourceId
     */
    void sendUpdate(BLETopics::NotifyByte topic, const std::function<nlohmann::json(uint32_t)>& packetMethod);

    /// Convenience overload so call sites can pass a build*Packet member function directly
    /// (e.g. &FrontendHandlerBlClass::buildPositionUpdatePacket) instead of writing a
    /// [this] lambda to bind it at every use.
    template <typename PacketT>
    void sendUpdate(const BLETopics::NotifyByte topic, PacketT (FrontendHandlerBlClass::*packetMethod)(uint32_t)) {
        sendUpdate(topic, [this, packetMethod](const uint32_t sourceId) { return (this->*packetMethod)(sourceId); });
    }

    /// Builds and sends the current packet for a single topic. Used both by the
    /// periodic broadcast below and by requestOutOfCycleUpdate's read-triggered refresh,
    /// so there's exactly one place that builds and dispatches each packet type.
    void sendUpdate(BLETopics::NotifyByte topic);

    /**
     * Enqueues a request to send an update for the given topic. This is used to handle read-triggered refreshes from the frontend.
     * @param topic
     */
    void requestOutOfCycleUpdate(BLETopics::NotifyByte topic);

    /**
     * Task entry point for sending data to the frontend over Bluetooth. This task runs indefinitely and handles sending updates to the frontend.
     * @param param
     */
    [[noreturn]] static void sendUpdateTaskEntry(void* param);

    /**
     * Task entry point for sending automatic updates. It dispatches updates for all topics in a loop to update data even if it was not manually requested.
     * @param param
     */
    [[noreturn]] static void sendAutomaticUpdateTaskEntry(void* param);
};

extern FrontendHandlerBlClass& FrontendHandlerBl;
