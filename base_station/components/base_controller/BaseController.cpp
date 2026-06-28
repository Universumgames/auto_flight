#include "BaseController.hpp"

#include "Barometer.hpp"
#include "FlightStorage.hpp"
#include "Frontend.hpp"
#include "GPS_Reader.hpp"
#include "i2c_manager.hpp"
#include "LoRa_Communication.hpp"
#include "Flight_Communication.hpp"

BaseControllerClass* instance = nullptr;

BaseControllerClass& BaseController = BaseControllerClass::getInstance();

const char* BaseControllerClass::TAG_BASE_CONTROLLER = "BaseController";

BaseControllerClass* BaseControllerClass::getInstancePtr() {
    if (instance == nullptr) {
        instance = new BaseControllerClass();
    }
    return instance;
}

BaseControllerClass& BaseControllerClass::getInstance() {
    return *getInstancePtr();
}


void BaseControllerClass::init() {
    I2CManager::getBus(); // initialize I2C bus

    FlightStorage.init();

    FrontendHandler.init();

    LoRa_Communication.begin();

    Barometer.begin();
    GPS_Reader.begin();

    xTaskCreate(loopTaskEntry, "BaseControllerLoop", 4096, this, tskIDLE_PRIORITY + 1, nullptr);

    LoRa_Communication.registerReceivePacketCallback([this](const LoRaPacket packet) {
        communicationCallback(packet);
    });

    FlightStorage.registerDataChangeCallback([]() {
        ESP_LOGI(TAG_BASE_CONTROLLER, "Planned area changed, sending update with size %d", FlightStorage.getPlannedArea().size());
        Flight_Communication::sendPlannedArea(FlightStorage.getPlannedArea());
    }, FlightStorageClass::DataUpdateType::AREA);
}


void BaseControllerClass::loopTaskEntry(void* param) {
    auto* instance = static_cast<BaseControllerClass*>(param);
    instance->loopTask();
}

[[noreturn]] void BaseControllerClass::loopTask() {
    while (true) {
        auto time = GPS_Reader.getGPSLatestTime();
        FlightStorage.updateBasePressure(Barometer.getPressure(), time);
        FlightStorage.updateBaseBarometerConnectionState(Barometer.available()
                                                             ? ConnectionState::CONNECTED
                                                             : ConnectionState::CONNECTING);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void BaseControllerClass::communicationCallback(LoRaPacket packet) {
    auto decodedPacket = Flight_Communication::decodePacket(packet);
    if (!decodedPacket) {
        ESP_LOGW(TAG_BASE_CONTROLLER, "Received invalid packet");
        return; // invalid packet, ignore
    }
    auto basePacket = decodedPacket.get();

    assert(((BasePacket*)packet.payload)->type == basePacket->type); // sanity check, should always hold
    ESP_LOGD(TAG_BASE_CONTROLLER, "Received packet of type 0x%02x at time %ld", basePacket->type,
             basePacket->timestamp);

    switch (basePacket->type) {
    case PacketType::ROUTE_HISTORY_REQUEST:
        break;
    case PacketType::SENSOR_UPDATE: {
        auto sensorUpdate = reinterpret_cast<SensorUpdate*>(decodedPacket.get());
        ESP_LOGI(TAG_BASE_CONTROLLER, "Received sensor update: pressure=%.2f", sensorUpdate->pressure);
        FlightStorage.updatePlanePressure(sensorUpdate->pressure, sensorUpdate->timestamp);
        break;
    }
    case PacketType::POSITION: {
        auto positionUpdate = reinterpret_cast<PositionUpdate*>(decodedPacket.get());
        ESP_LOGI(TAG_BASE_CONTROLLER, "Received position update: %s", positionUpdate->position.toString().c_str());
        FlightStorage.updatePlanePosition(positionUpdate->position, positionUpdate->timestamp);
        break;
    }
    case PacketType::PLANNED_ROUTE: {
        auto plannedRoute = reinterpret_cast<PlannedRoutePacket*>(decodedPacket.get());
        ESP_LOGI(TAG_BASE_CONTROLLER, "Received planned route with %d points",
                                plannedRoute->route.size());
        FlightStorage.updatePlannedRoute(plannedRoute->route, plannedRoute->timestamp);
        break;
    }
    case PacketType::COMPONENT_STATUS: {
        auto status = reinterpret_cast<ComponentStatus*>(decodedPacket.get());
        ESP_LOGI(TAG_BASE_CONTROLLER,
                                "Component status - GPS: %d, Barometer: %d, Gyroscope: %d, MotorControl: %d, Magnetometer: %d",
                                static_cast<int>(status->gps), static_cast<int>(status->barometer),
                                static_cast<int>(status->gyroscope), static_cast<int>(status->motorControl),
                                static_cast<int>(status->magnetometer));
        FlightStorage.updatePlaneBarometerConnectionState(status->barometer);
        FlightStorage.updatePlaneGyroscopeConnectionState(status->gyroscope);
        FlightStorage.updatePlaneMotorControlConnectionState(status->motorControl);
        FlightStorage.updatePlaneGPSConnectionState(status->gps);
        FlightStorage.updatePlaneMagnetometerConnectionState(status->magnetometer);
        break;
    }
    default:
        ESP_LOGW(TAG_BASE_CONTROLLER, "Unknown packet type: %02x", static_cast<int>(basePacket->type));
        break;
    }
}
