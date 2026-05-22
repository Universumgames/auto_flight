#pragma once
#include "mpu6050.h"


class FlightControllerClass {
private:
    FlightControllerClass();
public:
    ~FlightControllerClass() = delete;

    static FlightControllerClass& getInstance();
    static FlightControllerClass* getInstancePtr();

    void init();

private:

};


extern FlightControllerClass& FlightController;