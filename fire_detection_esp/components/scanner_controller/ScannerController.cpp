//
// Created by Tom Arlt on 22.10.26.
//

#include "ScannerController.hpp"

#include "CameraCommunication.hpp"
#include "scanner_i2c_manager.hpp"

static ScannerControllerClass *scannerControllerClass = nullptr;

ScannerControllerClass& ScannerController = ScannerControllerClass::getInstance();

ScannerControllerClass* ScannerControllerClass::getInstancePtr() {
    if (!scannerControllerClass) {
        scannerControllerClass = new ScannerControllerClass();
    }
    return scannerControllerClass;
}

ScannerControllerClass& ScannerControllerClass::getInstance() {
    return *getInstancePtr();
}

void ScannerControllerClass::init() {
    slaveHandle = ScannerI2CManager::getSlaveBus(CAMERA_CONTROLLER_I2C_ADDR);
}
