#pragma once
#include <cstdint>
#include <cstddef>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "BluetoothManager.hpp"

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

    Frontend::FlightUpdatePacket buildFlightUpdatePacket();
    Frontend::ConnectionUpdatePacket buildConnectionUpdatePacket();
    Frontend::SensorPacket buildSensorPacket();
    Frontend::BatteryStatusPacket buildBatteryStatusPacket();
    Frontend::PlannedRoutePacket buildPlannedRoutePacket();
    Frontend::AreaDefinePacket buildAreaDefinePacket();

    void sendUpdate(BLETopics::NotifyByte topic, const nlohmann::json& packet);
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
