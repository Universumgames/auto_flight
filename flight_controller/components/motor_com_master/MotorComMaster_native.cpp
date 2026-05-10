#include "MotorComMaster.hpp"

#ifdef NATIVE_BUILD
void MotorComMasterClass::init() {

}

ControlReturnCode MotorComMasterClass::sendCommand(ControlCommand command, uint8_t value) const {
    return ControlReturnCode::OK;
}

uint8_t MotorComMasterClass::readValue(ControlCommand command) const {
    return 0;
}

bool MotorComMasterClass::isSlaveConnected() {
    return true;
}
#endif