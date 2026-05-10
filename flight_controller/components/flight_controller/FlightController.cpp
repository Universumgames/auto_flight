//
// Created by Tom Arlt on 08.05.26.
//

#include "FlightController.hpp"

#include "mpu6050.h"

static FlightControllerClass* flight_controller = nullptr;

FlightControllerClass& FlightController = FlightControllerClass::getInstance();

FlightControllerClass* FlightControllerClass::getInstancePtr() {
    if (flight_controller == nullptr) {
        flight_controller = new FlightControllerClass();
    }
    return flight_controller;
}

FlightControllerClass& FlightControllerClass::getInstance() {
    return *getInstancePtr();
}

FlightControllerClass::FlightControllerClass() = default;

void FlightControllerClass::init() {
    //mpu6050_init_desc(&dev)
}
