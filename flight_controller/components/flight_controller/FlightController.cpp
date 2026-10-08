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

#include "Battery.hpp"
#include "geo_helper.hpp"
#include "route_planner.hpp"
#include "MotorComMaster.hpp"
#include "Magnetometer.hpp"
#include "OledDisplay.hpp"

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

    GPS_Reader.begin();
    MotorComMaster.init();
    Magnetometer.begin();
    Gyroscope.begin();
    Barometer.begin();
    Battery.begin();
    OledDisplay.begin();

    Flight_Communication::begin();

    xTaskCreate(flightTaskEntry, "FlightControllerFlightTask", 4096, this, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(sendUpdateTaskEntry, "FlightControllerSendUpdateTask", 4096, this, 10, nullptr);

    LoRa_Communication.registerReceivePacketCallback([this](const LoRaPacket packet) {
        communicationCallback(packet);
    });

    FlightStorage.registerDataChangeCallback([](uint32_t) {
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Planned route changed, sending update with size %d",
                 FlightStorage.getPlannedRoute().getRoutePoints().size());
        Flight_Communication::sendPlannedRoute();
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Planned route sent");
    }, FlightStorageClass::DataUpdateType::ROUTE);

    vTaskDelay(pdMS_TO_TICKS(200));
}

void FlightControllerClass::flightTaskEntry(void* param) {
    auto* instance = static_cast<FlightControllerClass*>(param);
    instance->flightTask();
}

[[noreturn]] void FlightControllerClass::flightTask() {
    while (true) {
        if (plannedAreaChanged) {
            recalculateRoute(FlightStorage.getPlannedArea());
        }

        auto currentTime = GPS_Reader.getGPSLatestTime();
        auto batteryPercentage = Battery.getVoltagePercentage();
        FlightStorage.updatePlaneBatteryPercentage(batteryPercentage, currentTime);
        //auto planeAngle = Gyroscope.getAngle();
        auto position = FlightStorage.updatePlanePosition(GPS_Reader.getCurrentPosition(), currentTime);
        auto pressure = FlightStorage.updatePlanePressure(Barometer.getPressure(), currentTime);
        vTaskDelay(1);
        auto groundPressure = FlightStorage.getBasePressure();
        auto currentAltitude = altitudeAverage.push(BarometerClass::calculateAltitude(groundPressure, pressure));
        auto planeAngle = Gyroscope.getGroundAngle();
        planeAngle.roll = rollAverage.push(planeAngle.roll);
        planeAngle.pitch = pitchAverage.push(planeAngle.pitch);
        auto compassHeading = headingAverage.push(Magnetometer.getHeading());

        //ESP_LOGI("GNDANG", "roll: %f, pitch: %f, yaw deg: %f", planeAngle.roll, planeAngle.pitch, planeAngle.yaw);
        //ESP_LOGI("MAGN", "x: %.1f mG, y: %.1f mG, z: %.1f mG, heading: %.1f deg, ready: %d, overflow: %d", magnetHeading.x, magnetHeading.y, magnetHeading.z, compassHeading, Magnetometer.isAvailable(), Magnetometer.isOverflow());

        updateConnectionStates();

        // advance waypoint index if close enough, then steer
        checkAndAdvanceWaypoint(position);

        steerToWaypoint(getNextWaypoint(), currentAltitude, planeAngle, compassHeading);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void FlightControllerClass::updateConnectionStates() {
    FlightStorage.updatePlaneGPSConnectionState(GPS_Reader.hasValidPosition()
                                                    ? ConnectionState::CONNECTED
                                                    : ConnectionState::CONNECTING);
    FlightStorage.updatePlaneMotorControlConnectionState(MotorComMaster.isSlaveConnected()
                                                             ? ConnectionState::CONNECTED
                                                             : ConnectionState::CONNECTING);
    FlightStorage.updatePlaneManualOverride(MotorComMaster.isManualOverride());
    FlightStorage.updatePlaneMagnetometerConnectionState(Magnetometer.isAvailable()
                                                             ? ConnectionState::CONNECTED
                                                             : ConnectionState::CONNECTING);
    FlightStorage.updatePlaneAccelerometerConnectionState(Gyroscope.initialized()
                                                              ? ConnectionState::CONNECTED
                                                              : ConnectionState::CONNECTING);
    FlightStorage.updatePlaneBarometerConnectionState(Barometer.available()
                                                          ? ConnectionState::CONNECTED
                                                          : ConnectionState::CONNECTING);
    FlightStorage.updatePlaneBatteryConnectionState(Battery.isAvailable()
                                                    ? ConnectionState::CONNECTED
                                                    : ConnectionState::CONNECTING);
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

    ESP_LOGD(TAG_FLIGHT_CONTROLLER, "Received packet of type 0x%02x at time %ld", basePacket->type,
             basePacket->timestamp);

    if (basePacket->id != 0 && basePacket->id != DeviceId::get32()) {
        ESP_LOGW(TAG_FLIGHT_CONTROLLER, "Received packet with unexpected source/destination ID: %u, expected: %u",
                 basePacket->id, DeviceId::get32());
        return; // ignore packets from other planes
    }

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
        if (plannedArea->shape.size() == FlightStorage.getPlannedArea().areaPoints.size() && std::equal(
            plannedArea->shape.begin(), plannedArea->shape.end(), FlightStorage.getPlannedArea().areaPoints.begin(),
            FlightStorage.getPlannedArea().areaPoints.end(), [](const Coordinate& a, const Coordinate& b) {
                return a == b;
            })) {
            ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Received planned area is the same as current, resending planned route");
            Flight_Communication::sendPlannedRoute();
            break;
        }
        FlightStorage.updatePlannedArea(AreaData{plannedArea->shape, plannedArea->settings});
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
    if (nextWaypointIndex < plannedRoute.getRoutePoints().size()) {
        return plannedRoute.getRoutePoints()[nextWaypointIndex];
    }
    return COORDINATE_INIT_INVALID();
}


void FlightControllerClass::recalculateRoute(const AreaData& newPlannedArea) {
    plannedAreaChanged = false;
    if (newPlannedArea.areaPoints.empty()) {
        ESP_LOGE(TAG_FLIGHT_CONTROLLER, "Cannot recalculate route: planned area is empty");
        return;
    }
    auto plannedRoute = RoutePlanner.planRoute(newPlannedArea.areaPoints, newPlannedArea.settings.routeAlgorithm, 60,
                                               metersToLatitudeDegree(40),
                                               (float)newPlannedArea.settings.overlapPercentage / 100.0f);
    FlightStorage.updatePlannedRoute(RouteData{plannedRoute, newPlannedArea.settings});
    nextWaypointIndex = 0;
    FlightStorage.updateFlightState(FlightState::FLYING);
}


void FlightControllerClass::checkAndAdvanceWaypoint(Coordinate currentPosition) {
    if (Coordinate::isInvalid(currentPosition)) return;

    auto plannedRoute = FlightStorage.getPlannedRoute();
    if (nextWaypointIndex >= plannedRoute.getRoutePoints().size()) {
        FlightStorage.updateFlightState(FlightState::RETURNING);
        return;
    }

    const Coordinate waypoint = plannedRoute.getRoutePoints()[nextWaypointIndex];
    if (distanceInMeters(currentPosition, waypoint) <= CONFIG_WAYPOINT_REACHED_RADIUS_M) {
        nextWaypointIndex++;
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Waypoint %d reached, advancing to %d/%d",
                 nextWaypointIndex - 1, nextWaypointIndex, plannedRoute.getRoutePoints().size());
    }
}

void FlightControllerClass::steerToWaypoint(Coordinate waypoint, int height, GyroscopeClass::GroundAngle angle,
                                            float compassHeading) {
    ESP_LOGD(TAG_FLIGHT_CONTROLLER, "Steering to waypoint: %s", waypoint.toString().c_str());
    // --- Rudder: steer toward waypoint ---
    int8_t rudder = 0;
    if (FlightStorage.getFlightState() == FlightState::FLYING && !Coordinate::isInvalid(waypoint)) {
        auto currentPosition = GPS_Reader.getCurrentPosition();
        if (!Coordinate::isInvalid(currentPosition)) {
            const float bearing = SteeringLaw::computeBearing(currentPosition, waypoint);
            const float headingErrorDeg = SteeringLaw::headingError(bearing, compassHeading);
            rudder = SteeringLaw::computeRudder(headingErrorDeg, CONFIG_RUDDER_SERVO_MIN, CONFIG_RUDDER_SERVO_MAX);
            ESP_LOGD(TAG_FLIGHT_CONTROLLER, "Rudder: bearing=%.1f heading=%.1f error=%.1f rudder=%d",
                     bearing, compassHeading, headingErrorDeg, rudder);
        }
    }

    // --- Altitude hold: PI on altitude error drives thrust and sets target pitch ---
    const float altitudeError = targetAltitude - static_cast<float>(height);
    const auto [thrust, targetPitch] = altitudeHold.update(altitudeError);

    ESP_LOGD(TAG_FLIGHT_CONTROLLER, "AltHold: alt=%d target=%.0f err=%.1f targetPitch=%.1f thrust=%d",
             height, targetAltitude, altitudeError, targetPitch, thrust);

    // --- Aileron/Pitch: keep wings level and drive toward targetPitch ---
    const float pitchError = targetPitch - angle.pitch;
    const auto [aileron, pitch] = attitude.update(angle.roll, pitchError);
    ESP_LOGD(TAG_FLIGHT_CONTROLLER, "Aileron: roll=%.1f out=%d", angle.roll, aileron);
    ESP_LOGD(TAG_FLIGHT_CONTROLLER, "Pitch: pitch=%.1f target=%.1f err=%.1f out=%d",
             angle.pitch, targetPitch, pitchError, pitch);

    MotorComMaster.sendFullControlPacket(aileron, pitch, thrust, rudder);
}
