#include "CameraComMaster.hpp"

#ifndef NATIVE_BUILD
#include "CameraCommunication.hpp"
#include "i2c_bus.h"
#include "i2c_manager.hpp"

static CameraComMasterClass *cameraComMasterClass = nullptr;

CameraComMasterClass& CameraComMaster = CameraComMasterClass::getInstance();

CameraComMasterClass* CameraComMasterClass::getInstancePtr() {
    if (!cameraComMasterClass) {
        cameraComMasterClass = new CameraComMasterClass();
    }
    return cameraComMasterClass;
}

CameraComMasterClass& CameraComMasterClass::getInstance() {
    return *getInstancePtr();
}

void CameraComMasterClass::init() {
    devHandle = i2c_bus_device_create(I2CManager::getBus(), CAMERA_CONTROLLER_I2C_ADDR, i2c_bus_get_current_clk_speed(I2CManager::getBus()));
}

bool CameraComMasterClass::isSlaveConnected() {
    if (!devHandle) {
        return false;
    }
    uint8_t resultReady;
    errorCode = i2c_bus_read_byte(devHandle, CameraCommunication::ScanI2CCommand::EMPTY, &resultReady);
    return errorCode == ESP_OK && resultReady == 1;
}

bool CameraComMasterClass::isResultReady() {
    uint8_t resultReady = 0;
    errorCode = i2c_bus_read_byte(devHandle, CameraCommunication::ScanI2CCommand::SCAN_RESULT_READY, &resultReady);
    if (errorCode != ESP_OK) {
        return false;
    }
    return resultReady == 1;
}

std::optional<CameraCommunication::ScanResult> CameraComMasterClass::getScanResult() {
    if (!isResultReady()) {
        return std::nullopt;
    }
    return CameraCommunication::receiveScanResult(devHandle);
}

void CameraComMasterClass::requestScan(const CameraCommunication::ScanRequest& request) {
    if (!isSlaveConnected()) {
        ESP_LOGE("CameraComMaster", "Cannot request scan: Camera slave not connected");
        return;
    }
    CameraCommunication::sendScanRequest(request, devHandle);
}
#endif