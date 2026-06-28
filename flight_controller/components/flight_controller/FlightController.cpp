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
#include "Magnetometer.hpp"

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
    Magnetometer.begin();
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

int i = -100;

[[noreturn]] void FlightControllerClass::flightTask() {
    while (true) {
        if (plannedAreaChanged) {
            recalculateRoute();
        }

        auto currentTime = GPS_Reader.getGPSLatestTime();
        auto hasControl = !MotorComMaster.isManualOverride();
        //auto planeAngle = Gyroscope.getAngle();
        auto position = FlightStorage.updatePlanePosition(GPS_Reader.getCurrentPosition(), currentTime);
        auto pressure = FlightStorage.updatePlanePressure(Barometer.getPressure(), currentTime);
        auto groundPressure = FlightStorage.getBasePressure();
        auto currentAltitude = BarometerClass::calculateAltitude(groundPressure, pressure);
        auto planeAngle = Gyroscope.getPlaneAngle();
        auto magnetHeading = Magnetometer.readData();
        auto compassHeading = Magnetometer.getHeading();

        //ESP_LOGI("GNDANG", "roll: %f, pitch: %f, yaw deg: %f", planeAngle.roll, planeAngle.pitch, planeAngle.yaw);
        ESP_LOGI("MAGN", "x: %.1f mG, y: %.1f mG, z: %.1f mG, heading: %.1f deg, ctrl1: 0x%02x, ctrl2: 0x%02x, ready: %d", magnetHeading.x, magnetHeading.y, magnetHeading.z, compassHeading, Magnetometer.getRegCTRL1(), Magnetometer.getRegCTRL2(), Magnetometer.isAvailable());

        FlightStorage.updatePlaneGPSConnectionState(GPS_Reader.hasValidPosition()
                                                        ? ConnectionState::CONNECTED
                                                        : ConnectionState::CONNECTING);
        FlightStorage.updatePlaneBarometerConnectionState(Barometer.available()
                                                              ? ConnectionState::CONNECTED
                                                              : ConnectionState::CONNECTING);
        //FlightStorage.updatePlaneGyroscopeConnectionState(Gyroscope.available() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING);
        FlightStorage.updatePlaneMotorControlConnectionState(
            MotorComMaster.isSlaveConnected() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING);
        FlightStorage.updatePlaneMagnetometerConnectionState(
            Magnetometer.isAvailable() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING);
        FlightStorage.updatePlaneMotorControlConnectionState(
            MotorComMaster.isSlaveConnected() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING);

        // advance waypoint index if close enough, then steer
        checkAndAdvanceWaypoint(position);

        //steerToWaypoint(getNextWaypoint(), currentAltitude, planeAngle, compassHeading);
        MotorComMaster.sendFullControlPacket(i,i,i,i);
        i++;
        if (i > 100) i = -100;
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


void FlightControllerClass::checkAndAdvanceWaypoint(Coordinate currentPosition) {
    if (Coordinate::isInvalid(currentPosition)) return;

    auto plannedRoute = FlightStorage.getPlannedRoute();
    if (nextWaypointIndex >= plannedRoute.size()) return;

    const Coordinate waypoint = plannedRoute[nextWaypointIndex];
    if (distanceInMeters(currentPosition, waypoint) <= CONFIG_WAYPOINT_REACHED_RADIUS_M) {
        nextWaypointIndex++;
        ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Waypoint %d reached, advancing to %d/%d",
                 nextWaypointIndex - 1, nextWaypointIndex, plannedRoute.size());
    }
}

void FlightControllerClass::steerToWaypoint(Coordinate waypoint, int height, GyroscopeClass::PlaneAngle angle, float compassHeading) {
    // --- Rudder: steer toward waypoint ---
    int8_t rudder = 0;
    if (!Coordinate::isInvalid(waypoint)) {
        auto currentPosition = GPS_Reader.getCurrentPosition();
        if (!Coordinate::isInvalid(currentPosition)) {
            const float dLon = (waypoint.longitude - currentPosition.longitude) * M_PI / 180.0f;
            const float lat1 = currentPosition.latitude * M_PI / 180.0f;
            const float lat2 = waypoint.latitude * M_PI / 180.0f;
            const float y = sinf(dLon) * cosf(lat2);
            const float x = cosf(lat1) * sinf(lat2) - sinf(lat1) * cosf(lat2) * cosf(dLon);
            float bearing = atan2f(y, x) * 180.0f / M_PI;
            bearing = fmodf(bearing + 360.0f, 360.0f);

            float headingError = bearing - compassHeading;
            if (headingError >= 180.0f) headingError -= 360.0f;
            if (headingError < -180.0f) headingError += 360.0f;

            // normalize to [-1, 1]: full deflection at ±90° heading error
            const float normalized = std::max(-1.0f, std::min(1.0f, headingError / 90.0f));
            const float rudderF = normalized >= 0.0f
                ? normalized * static_cast<float>(CONFIG_RUDDER_SERVO_MAX)
                : normalized * static_cast<float>(-CONFIG_RUDDER_SERVO_MIN);
            rudder = static_cast<int8_t>(rudderF);
            ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Rudder: bearing=%.1f heading=%.1f error=%.1f rudder=%d",
                     bearing, compassHeading, headingError, rudder);
        }
    }

    // --- Altitude hold: PI on altitude error drives thrust and sets target pitch ---
    const float altitudeError = targetAltitude - static_cast<float>(height);
    thrustIntegral = std::max(-THRUST_I_LIMIT, std::min(THRUST_I_LIMIT,
                               thrustIntegral + altitudeError * 0.05f));
    const float thrustF = THRUST_BASE + altitudeError * THRUST_P_GAIN + thrustIntegral * THRUST_I_GAIN;
    const auto thrust = static_cast<int8_t>(std::max(0.0f, std::min(100.0f, thrustF)));

    // Target pitch is derived from altitude error; I term on pitch finds the unknown trim.
    const float targetPitch = std::max(-MAX_CLIMB_PITCH, std::min(MAX_CLIMB_PITCH,
                                        altitudeError * ALTITUDE_TO_PITCH_GAIN));

    ESP_LOGI(TAG_FLIGHT_CONTROLLER, "AltHold: alt=%d target=%.0f err=%.1f targetPitch=%.1f thrust=%d",
             height, targetAltitude, altitudeError, targetPitch, thrust);

    // --- Aileron: keep wings level, I term discovers roll trim offset ---
    aileronIntegral = std::max(-AILERON_I_LIMIT, std::min(AILERON_I_LIMIT,
                                aileronIntegral + angle.roll * 0.05f));
    const float aileronF = angle.roll * AILERON_P_GAIN + aileronIntegral * AILERON_I_GAIN;
    const auto aileron = static_cast<int8_t>(std::max(-100.0f, std::min(100.0f, aileronF)));
    ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Aileron: roll=%.1f integral=%.2f out=%d",
             angle.roll, aileronIntegral, aileron);

    // --- Pitch: drive toward targetPitch, I term discovers pitch trim offset ---
    const float pitchError = targetPitch - angle.pitch;
    pitchIntegral = std::max(-PITCH_I_LIMIT, std::min(PITCH_I_LIMIT,
                              pitchIntegral + pitchError * 0.05f));
    const float pitchF = pitchError * PITCH_P_GAIN + pitchIntegral * PITCH_I_GAIN;
    const auto pitch = static_cast<int8_t>(std::max(-100.0f, std::min(100.0f, pitchF)));
    ESP_LOGI(TAG_FLIGHT_CONTROLLER, "Pitch: pitch=%.1f target=%.1f err=%.1f integral=%.2f out=%d",
             angle.pitch, targetPitch, pitchError, pitchIntegral, pitch);

    MotorComMaster.sendFullControlPacket(aileron, pitch, thrust, rudder);
}
