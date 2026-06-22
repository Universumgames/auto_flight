//
// Created by Tom Arlt on 08.05.26.
//

#include "FlightController.hpp"

#include "Barometer.hpp"
#include "FlightStorage.hpp"
#include "GPS_Reader.hpp"
#include "Gyroscope.hpp"
#include "i2c_manager.hpp"
#include "mpu6050.h"
#include "Flight_Communication.hpp"
#include "LoRa_Communication.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <variant>

#include "geo_helper.hpp"
#include "route_planner.hpp"
#include "MotorComMaster.hpp"

static FlightControllerClass* flight_controller = nullptr;

FlightControllerClass& FlightController = FlightControllerClass::getInstance();

const char* FlightControllerClass::TAG_FLIGHT_CONTROLLER = "FlightController";

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
    I2CManager::getBus(); // initialize I2C bus

    FlightStorage.init();

    LoRa_Communication.begin();

    Gyroscope.begin();
    Barometer.begin();
    GPS_Reader.begin();
    MotorComMaster.init();

    Flight_Communication::begin();

    xTaskCreate(flightTaskEntry, "FlightControllerFlightTask", 4096, this, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(sendUpdateTaskEntry, "FlightControllerSendUpdateTask", 4096, this, 10, nullptr);

    LoRa_Communication.registerReceivePacketCallback([this](const LoRaPacket packet) {
        communicationCallback(packet);
    });

    FlightStorage.registerDataChangeCallback([]() {
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Planned route changed, sending update with size %d",
                 FlightStorage.getPlannedRoute().size());
        Flight_Communication::sendPlannedRoute();
    }, FlightStorageClass::DataUpdateType::ROUTE);
}

void FlightControllerClass::flightTaskEntry(void* param) {
    auto* instance = static_cast<FlightControllerClass*>(param);
    instance->flightTask();
}

[[noreturn]] void FlightControllerClass::flightTask() {
    while (true) {
        if (plannedAreaChanged) {
            recalculateRoute();
        }

        auto currentTime = GPS_Reader.getGPSLatestTime();
        //auto planeAngle = Gyroscope.getAngle();
        auto position = FlightStorage.updatePlanePosition(GPS_Reader.getCurrentPosition(), currentTime);
        auto pressure = FlightStorage.updatePlanePressure(Barometer.getPressure(), currentTime);
        auto groundPressure = FlightStorage.getBasePressure();
        auto currentAltitude = BarometerClass::calculateAltitude(groundPressure, pressure);
        auto planeAngle = Gyroscope.getPlaneAngle();

        ESP_LOGI("GNDANG", "roll: %f, pitch: %f, yaw deg: %f", planeAngle.roll, planeAngle.pitch, planeAngle.yaw);

        FlightStorage.updatePlaneGPSConnectionState(GPS_Reader.hasValidPosition()
                                                        ? ConnectionState::CONNECTED
                                                        : ConnectionState::CONNECTING);
        FlightStorage.updatePlaneBarometerConnectionState(Barometer.available()
                                                              ? ConnectionState::CONNECTED
                                                              : ConnectionState::CONNECTING);
        //FlightStorage.updatePlaneGyroscopeConnectionState(Gyroscope.available() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING);
        FlightStorage.updatePlaneMotorControlConnectionState(
            MotorComMaster.isSlaveConnected() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING);

        // steer to next waypoint, keep current height
        steerToWaypoint(getNextWaypoint(), currentAltitude, planeAngle);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void FlightControllerClass::sendUpdateTaskEntry(void* param) {
    auto* instance = static_cast<FlightControllerClass*>(param);
    instance->sendUpdateTask();
}

[[noreturn]] void FlightControllerClass::sendUpdateTask() {
    while (true) {
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Sending updates: position, sensor, component status");
        Flight_Communication::sendPosition();
        Flight_Communication::sendSensorUpdate();
        Flight_Communication::sendComponentStatus();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SEND_UPDATE_INTERVAL_MS));
    }
}

void FlightControllerClass::communicationCallback(LoRaPacket packet) {
    auto decodedPacket = Flight_Communication::decodePacket(packet);
    if (!decodedPacket) {
        ESP_LOGW(TAG_FLIGHT_CONTROLLER, "Received invalid packet");
        return; // invalid packet, ignore
    }
    auto basePacket = decodedPacket.get();

    assert(((BasePacket*)packet.payload)->type == basePacket->type); // sanity check, should always hold
    ESP_LOGD(TAG_FLIGHT_CONTROLLER, "Received packet of type 0x%02x at time %ld", basePacket->type,
             basePacket->timestamp);

    switch (basePacket->type) {
    case PacketType::SENSOR_UPDATE: {
        auto sensorUpdate = reinterpret_cast<SensorUpdate*>(decodedPacket.get());
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Received sensor update: pressure=%.2f", sensorUpdate->pressure);
        FlightStorage.updateBasePressure(sensorUpdate->pressure, sensorUpdate->timestamp);
        break;
    }
    case PacketType::POSITION: {
        auto positionUpdate = reinterpret_cast<PositionUpdate*>(decodedPacket.get());
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Received position update: %s", positionUpdate->position.toString().c_str());
        FlightStorage.updateBasePosition(positionUpdate->position, positionUpdate->timestamp);
        break;
    }
    case PacketType::PLANNED_AREA: {
        auto plannedArea = reinterpret_cast<PlannedAreaPacket*>(decodedPacket.get());
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Received planned area with %d points", plannedArea->shape.size());
        if (plannedArea->shape.size() == FlightStorage.getPlannedArea().size() && std::equal(
            plannedArea->shape.begin(), plannedArea->shape.end(), FlightStorage.getPlannedArea().begin(),
            FlightStorage.getPlannedArea().end(), [](const Coordinate& a, const Coordinate& b) { return a == b; })) {
            ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Received planned area is the same as current, ignoring");
            break;
        }
        FlightStorage.updatePlannedArea(plannedArea->shape);
        plannedAreaChanged = true;
        break;
    }
    default:
        ESP_LOGW(TAG_FLIGHT_CONTROLLER, "Unknown packet type: %xd", basePacket->type);
        break;
    }
}


Coordinate FlightControllerClass::getNextWaypoint() const {
    auto plannedRoute = FlightStorage.getPlannedRoute();
    if (nextWaypointIndex < plannedRoute.size()) {
        return plannedRoute[nextWaypointIndex];
    }
    return COORDINATE_INIT_INVALID();
}


void FlightControllerClass::recalculateRoute() {
    plannedAreaChanged = false;
    if (FlightStorage.getPlannedArea().empty()) {
        ESP_LOGE(TAG_FLIGHT_CONTROLLER, "Cannot recalculate route: planned area is empty");
        return;
    }
    auto plannedRoute = RoutePlanner.planRoute(FlightStorage.getPlannedArea(), 60, metersToLatitudeDegree(40), 0.2);
    FlightStorage.updatePlannedRoute(plannedRoute);
    nextWaypointIndex = 0;
}


void FlightControllerClass::steerToWaypoint(Coordinate waypoint, int height, GyroscopeClass::PlaneAngle angle) {}
