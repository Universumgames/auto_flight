#pragma once
#ifndef NATIVE_BUILD
#include "i2c_bus.h"
#endif
#include <optional>

#include "CameraCommunication.hpp"

class CameraComMasterClass {

private:
    CameraComMasterClass() = default;
public:
    ~CameraComMasterClass() = delete;
public:
    static CameraComMasterClass *getInstancePtr();
    static CameraComMasterClass& getInstance();

    void init();

    [[nodiscard]] bool isSlaveConnected();

    bool isResultReady();

    std::optional<CameraCommunication::ScanResult> getScanResult();

    void requestScan(const CameraCommunication::ScanRequest& request);

private:
#ifndef NATIVE_BUILD
    i2c_bus_device_handle_t devHandle = nullptr;
    esp_err_t errorCode = ESP_FAIL;
#endif

};

extern CameraComMasterClass& CameraComMaster;
