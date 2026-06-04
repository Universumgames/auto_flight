#pragma once
#include "LoRa_Communication.hpp"

class BaseControllerClass {
private:
    BaseControllerClass() = default;

    static const char* TAG_BASE_CONTROLLER;
public:
    ~BaseControllerClass() = delete;

    static BaseControllerClass* getInstancePtr();
    static BaseControllerClass& getInstance();

public:
    void init();

private:
    static void loopTaskEntry(void* param);

    [[noreturn]] void loopTask();

    void communicationCallback(LoRaPacket packet);

};

extern BaseControllerClass& BaseController;