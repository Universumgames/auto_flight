#include "CameraComMaster.hpp"

#ifdef NATIVE_BUILD
#include "CameraCommunication.hpp"

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
}

bool CameraComMasterClass::isSlaveConnected() {
    return false;
}

bool CameraComMasterClass::isResultReady() {
    return false;
}

std::optional<CameraCommunication::ScanResult> CameraComMasterClass::getScanResult() {
    return std::nullopt;
}

void CameraComMasterClass::requestScan(const CameraCommunication::ScanRequest& request) {

}
#endif