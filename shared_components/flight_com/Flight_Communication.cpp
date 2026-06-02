#include "Flight_Communication.hpp"

#include "Barometer.hpp"
#include "FlightStorage.hpp"
#include "GPS_Reader.hpp"
#include "LoRa_Communication.hpp"
#ifdef FLIGHT_DEVICE_TYPE_PLANE
#include "MotorComMaster.hpp"
#endif
#include "Gyroscope.hpp"

constexpr const char* TAG_FLIGHT_COMMUNICATION = "Flight_Communication";

void Flight_Communication::begin() {}

std::unique_ptr<FlightPacket> Flight_Communication::decodePacket(const LoRaPacket& packet) {
    return decodePacket(packet.payload, packet.length);
}

std::unique_ptr<FlightPacket> Flight_Communication::decodePacket(const uint8_t* data, std::size_t len) {
    if (len < 1) {
        return nullptr; // invalid packet
    }

    BasePacket basePacket = {};
    std::memcpy(&basePacket, data, sizeof(BasePacket));

    switch (basePacket.type) {
    case PacketType::SENSOR_UPDATE: {
        if (len < sizeof(SensorUpdate)) {
            return nullptr; // invalid packet
        }
        SensorUpdate sensorUpdate = {};
        std::memcpy(&sensorUpdate, data, sizeof(SensorUpdate));
        return std::make_unique<FlightPacket>(sensorUpdate);
    }
    case PacketType::POSITION: {
        if (len < sizeof(PositionUpdate)) {
            return nullptr; // invalid packet
        }
        PositionUpdate positionUpdate = {};
        std::memcpy(&positionUpdate, data, sizeof(PositionUpdate));
        return std::make_unique<FlightPacket>(positionUpdate);
    }
    case PacketType::ROUTE_HISTORY_REQUEST: {
        return std::make_unique<FlightPacket>(basePacket);
    }
    case PacketType::PLANNED_AREA: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            return nullptr; // invalid packet
        }
        size_t length = 0;
        std::memcpy(&length, data + sizeof(BasePacket), sizeof(size_t));
        if (len < sizeof(BasePacket) + sizeof(size_t) + length * sizeof(Coordinate)) {
            return nullptr; // invalid packet
        }
        PlannedAreaPacket plannedArea = {};
        std::memcpy(&plannedArea, data, sizeof(BasePacket));
        plannedArea.shape.resize(length);
        std::memcpy(plannedArea.shape.data(), data + sizeof(BasePacket) + sizeof(size_t), length * sizeof(Coordinate));
        return std::make_unique<FlightPacket>(plannedArea);
    }
    case PacketType::ROUTE_HISTORY: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            return nullptr;
        }
        size_t length = 0;
        std::memcpy(&length, data + sizeof(BasePacket), sizeof(size_t));
        if (len < sizeof(BasePacket) + sizeof(size_t) + length * sizeof(Coordinate)) {
            return nullptr;
        }
        FlightHistoryPacket flightHistory = {};
        std::memcpy(&flightHistory, data, sizeof(BasePacket));
        flightHistory.history.resize(length);
        std::memcpy(flightHistory.history.data(), data + sizeof(BasePacket) + sizeof(size_t),
                    length * sizeof(Coordinate));
        return std::make_unique<FlightPacket>(flightHistory);
    }
    case PacketType::PLANNED_ROUTE: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            return nullptr;
        }
        size_t length = 0;
        std::memcpy(&length, data + sizeof(BasePacket), sizeof(size_t));
        if (len < sizeof(BasePacket) + sizeof(size_t) + length * sizeof(Coordinate)) {
            return nullptr;
        }
        PlannedRoutePacket plannedRoute = {};
        std::memcpy(&plannedRoute, data, sizeof(BasePacket));
        plannedRoute.route.resize(length);
        std::memcpy(plannedRoute.route.data(), data + sizeof(BasePacket) + sizeof(size_t), length * sizeof(Coordinate));
        return std::make_unique<FlightPacket>(plannedRoute);
    }
    case PacketType::COMPONENT_STATUS: {
        if (len < sizeof(ComponentStatus)) {
            return nullptr; // invalid packet
        }
        ComponentStatus status = {};
        std::memcpy(&status, data, sizeof(ComponentStatus));
        return std::make_unique<FlightPacket>(status);
    }
    default:
        ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Unknown packet type: %xd", static_cast<int>(basePacket.type));
        return nullptr;
    }
}

void Flight_Communication::sendPosition() {
    Coordinate position = GPS_Reader.getCurrentPosition();
    PositionUpdate packet = {};
    packet.timestamp = GPS_Reader.getGPSLatestTime();
    packet.type = PacketType::POSITION;
    packet.position = position;

    LoRa_Communication.sendData(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
}

void Flight_Communication::sendSensorUpdate() {
    float pressure = Barometer.getPressure();
    SensorUpdate packet = {};
    packet.timestamp = GPS_Reader.getGPSLatestTime();
    packet.type = PacketType::SENSOR_UPDATE;
    packet.pressure = pressure;

    LoRa_Communication.sendData(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
}

#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
void Flight_Communication::requestRouteHistory() {
    BasePacket packet = {};
    packet.timestamp = GPS_Reader.getGPSLatestTime();
    packet.type = PacketType::ROUTE_HISTORY_REQUEST;

    LoRa_Communication.sendData(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
}

void Flight_Communication::sendPlannedArea(std::vector<Coordinate> shape) {
    BasePacket packet = {};
    packet.timestamp = GPS_Reader.getGPSLatestTime();
    packet.type = PacketType::PLANNED_AREA;
    size_t length = shape.size();
    size_t dataSize = sizeof(BasePacket) + sizeof(int) + shape.size() * sizeof(Coordinate);
    auto data = new uint8_t[dataSize];
    uint8_t dataOffset = 0;
    memccpy(data, &packet, 1, sizeof(BasePacket));
    dataOffset += sizeof(BasePacket);
    memcpy(data + dataOffset, &length, sizeof(size_t));
    dataOffset += sizeof(size_t);
    memcpy(data + dataOffset, shape.data(), sizeof(Coordinate) * shape.size());

    LoRa_Communication.sendData(data, dataSize);
    delete[] data;
}
#endif

#ifdef FLIGHT_DEVICE_TYPE_PLANE
void Flight_Communication::sendRouteHistory() {
    FlightRoute flightRoute = FlightStorage.getFlightRoute();
    size_t length = flightRoute.size();
    size_t dataSize = sizeof(BasePacket) + sizeof(int) + flightRoute.size() * sizeof(Coordinate);
    auto data = new uint8_t[dataSize];
    BasePacket packet = {};
    packet.timestamp = GPS_Reader.getGPSLatestTime();
    packet.type = PacketType::ROUTE_HISTORY;
    uint8_t dataOffset = 0;
    memccpy(data, &packet, 1, sizeof(BasePacket));
    dataOffset += sizeof(BasePacket);
    memcpy(data + dataOffset, &length, sizeof(size_t));
    dataOffset += sizeof(size_t);
    memcpy(data + dataOffset, flightRoute.data(), sizeof(Coordinate) * flightRoute.size());

    LoRa_Communication.sendData(data, dataSize);
    delete[] data;
}

void Flight_Communication::sendPlannedRoute() {
    PlannedRoute plannedRoute = FlightStorage.getPlannedRoute();
    size_t length = plannedRoute.size();
    size_t dataSize = sizeof(BasePacket) + sizeof(int) + plannedRoute.size() * sizeof(Coordinate);
    auto data = new uint8_t[dataSize];
    BasePacket packet = {};
    packet.timestamp = GPS_Reader.getGPSLatestTime();
    packet.type = PacketType::PLANNED_ROUTE;
    uint8_t dataOffset = 0;
    memccpy(data, &packet, 1, sizeof(BasePacket));
    dataOffset += sizeof(BasePacket);
    memcpy(data + dataOffset, &length, sizeof(size_t));
    dataOffset += sizeof(size_t);
    memcpy(data + dataOffset, plannedRoute.data(), sizeof(Coordinate) * plannedRoute.size());

    LoRa_Communication.sendData(data, dataSize);
    delete[] data;
}

void Flight_Communication::sendComponentStatus() {
    ComponentStatus status = {};
    status.timestamp = GPS_Reader.getGPSLatestTime();
    status.type = PacketType::COMPONENT_STATUS;
    status.gps = GPS_Reader.hasValidPosition() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING;
    // for simplicity, we assume other components are always connected in this example
    status.barometer = Barometer.initialized() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING;
    status.gyroscope = Gyroscope.initialized() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING;
    status.motorControl = MotorComMaster.isSlaveConnected() ? ConnectionState::CONNECTED : ConnectionState::CONNECTING;

    LoRa_Communication.sendData(reinterpret_cast<const uint8_t*>(&status), sizeof(status));
}
#endif
