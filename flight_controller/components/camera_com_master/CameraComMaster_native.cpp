#include "CameraComMaster.hpp"

#ifdef NATIVE_BUILD
#include "CameraCommunication.hpp"

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