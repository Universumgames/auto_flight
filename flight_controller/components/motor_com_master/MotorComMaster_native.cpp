#include "MotorComMaster.hpp"

#ifdef NATIVE_BUILD
void MotorComMasterClass::init() {

}

bool MotorComMasterClass::sendCommand(ControlCommand command, uint8_t value) const {
    return true;
}

uint8_t MotorComMasterClass::readValue(ControlCommand command) const {
    return 0;
}

bool MotorComMasterClass::isSlaveConnected() {
    return true;
}

bool MotorComMasterClass::sendFullControlPacket(uint8_t aileronDiff, uint8_t pitch, uint8_t thrust, uint8_t rudder) {
    return true;
}

#endif