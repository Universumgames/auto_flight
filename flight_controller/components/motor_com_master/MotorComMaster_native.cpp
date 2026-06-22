#include "MotorComMaster.hpp"

#ifdef NATIVE_BUILD
void MotorComMasterClass::init() {

}

bool MotorComMasterClass::sendCommand(ControlCommand command, int8_t value) {
    return true;
}

bool MotorComMasterClass::isSlaveConnected() const {
    return true;
}

bool MotorComMasterClass::sendFullControlPacket(int8_t aileronDiff, int8_t pitch, int8_t thrust, int8_t rudder) {
    return true;
}

#endif