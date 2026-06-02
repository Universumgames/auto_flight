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

#include "route_planner.hpp"
#include "MotorComMaster.hpp"

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
    I2CManager::getBus(); // initialize I2C bus

    FlightStorage.init();

    LoRa_Communication.begin();

    Gyroscope.begin();
    Barometer.begin();
    GPS_Reader.begin();
    MotorComMaster.init();

    Flight_Communication::begin();

    xTaskCreate(flightTaskEntry, "FlightControllerFlightTask", 4096, this, tskIDLE_PRIORITY + 1, nullptr);

    LoRa_Communication.registerReceivePacketCallback([this](const LoRaPacket packet) {
        communicationCallback(packet);
    });
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
        auto planeAngle = Gyroscope.getAngle();
        auto position = FlightStorage.updatePlanePosition(GPS_Reader.getCurrentPosition(), currentTime);
        auto pressure = FlightStorage.updatePlanePressure(Barometer.getPressure(), currentTime);
        auto groundPressure = FlightStorage.getBasePressure();
        auto currentAltitude = BarometerClass::calculateAltitude(groundPressure, pressure);

        // steer to next waypoint, keep current height
        steerToWaypoint(getNextWaypoint(), currentAltitude);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void FlightControllerClass::sendUpdateTaskEntry(void* param) {
    auto* instance = static_cast<FlightControllerClass*>(param);
    instance->sendUpdateTask();
}

[[noreturn]] void FlightControllerClass::sendUpdateTask() {
    while (true) {
        Flight_Communication::sendPosition();
        Flight_Communication::sendSensorUpdate();
        Flight_Communication::sendComponentStatus();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SEND_UPDATE_INTERVAL_MS));
    }
}

void FlightControllerClass::communicationCallback(LoRaPacket packet) {
    auto decodedPacket = Flight_Communication::decodePacket(packet);
    if (!decodedPacket) {
        return; // invalid packet, ignore
    }

    // decode variant and get actual object
    std::visit(Overloaded{
                   [](const BasePacket& basePacket) {
                       switch (basePacket.type) {
                       case PacketType::ROUTE_HISTORY_REQUEST:
                           // handle route history request, send back route history
                           break;
                       default:
                           break; // ignore other base packets for now
                       }
                   },
                   [](const SensorUpdate& sensorUpdate) {
                       // handle sensor update
                       FlightStorage.updateBasePressure(sensorUpdate.pressure, sensorUpdate.timestamp);
                   },
                   [](const PositionUpdate& positionUpdate) {
                       // handle position update
                       FlightStorage.updateBasePosition(positionUpdate.position, positionUpdate.timestamp);
                   },
                   [this](const PlannedAreaPacket& plannedArea) {
                       FlightStorage.updatePlannedArea(plannedArea.shape);
                       plannedAreaChanged = true;
                   },
                   [](const FlightHistoryPacket& flightHistory) {
                       // cannot happen
                   },
                   [](const PlannedRoutePacket& plannedRoute) {
                       // cannot happen
                   }
               }, *decodedPacket);
}


Coordinate FlightControllerClass::getNextWaypoint() {
    auto plannedRoute = FlightStorage.getPlannedRoute();
    if (nextWaypointIndex < plannedRoute.size()) {
        return plannedRoute[nextWaypointIndex];
    }
    return COORDINATE_INIT_INVALID();
}


void FlightControllerClass::recalculateRoute() {
    plannedAreaChanged = false;
    auto plannedRoute = RoutePlanner.planRoute(FlightStorage.getPlannedArea(), 20, 40, 0.2);
    FlightStorage.updatePlannedRoute(plannedRoute);
    nextWaypointIndex = 0;
    Flight_Communication::sendPlannedRoute();
}


void FlightControllerClass::steerToWaypoint(Coordinate waypoint, int height) {

}
