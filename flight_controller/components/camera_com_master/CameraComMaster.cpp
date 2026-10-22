#include "CameraComMaster.hpp"

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
