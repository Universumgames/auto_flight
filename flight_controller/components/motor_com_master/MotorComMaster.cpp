#include "MotorComMaster.hpp"

static MotorComMasterClass *motorComMasterClass = nullptr;

MotorComMasterClass& MotorComMaster = MotorComMasterClass::getInstance();

MotorComMasterClass* MotorComMasterClass::getInstancePtr() {
    if (!motorComMasterClass) {
        motorComMasterClass = new MotorComMasterClass();
    }
    return motorComMasterClass;
}

MotorComMasterClass& MotorComMasterClass::getInstance() {
    return *getInstancePtr();
}
