#pragma once
#include <cstdint>
#include <cstddef>

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
public:
    void init();

private:

    void registerReadCallbacks();

    Frontend::FlightUpdatePacket buildFlightUpdatePacket();
    Frontend::ConnectionUpdatePacket buildConnectionUpdatePacket();
    Frontend::SensorPacket buildSensorPacket();
    Frontend::BatteryStatusPacket buildBatteryStatusPacket();
    Frontend::PlannedRoutePacket buildPlannedRoutePacket();
    Frontend::AreaDefinePacket buildAreaDefinePacket();

    void sendUpdate(BLETopics::NotifyByte topic, const nlohmann::json& packet);
    void sendUpdate();

    [[noreturn]] static void sendUpdateTaskEntry(void* param);
};

extern FrontendHandlerBlClass& FrontendHandlerBl;
