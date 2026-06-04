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

std::unique_ptr<BasePacket> Flight_Communication::decodePacket(const LoRaPacket& packet) {
    return decodePacket(packet.payload, packet.length);
}

std::unique_ptr<BasePacket> Flight_Communication::decodePacket(const uint8_t* data, std::size_t len) {
    if (len < sizeof(BasePacket)) {
        ESP_LOGE(TAG_FLIGHT_COMMUNICATION, "Received packet too small: %d bytes, should be at least %d bytes", len,
                 static_cast<int>(sizeof(BasePacket)));
        return nullptr; // invalid packet
    }

    switch (auto type = ((BasePacket*)data)->type) {
    case PacketType::SENSOR_UPDATE: {
        if (len < sizeof(SensorUpdate)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid SENSOR_UPDATE packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(SensorUpdate)));
            return nullptr; // invalid packet
        }
        auto sensorUpdate = new SensorUpdate(data);

        return std::unique_ptr<BasePacket>(sensorUpdate);
    }
    case PacketType::POSITION: {
        if (len < sizeof(PositionUpdate)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid POSITION packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(PositionUpdate)));
            return nullptr; // invalid packet
        }
        auto positionUpdate = new PositionUpdate(data);
        return std::unique_ptr<BasePacket>(positionUpdate);
    }
    case PacketType::ROUTE_HISTORY_REQUEST: {
        auto routeHistoryRequest = new BasePacket(data);
        return std::unique_ptr<BasePacket>(routeHistoryRequest);
    }
    case PacketType::PLANNED_AREA: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid PLANNED_AREA packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(BasePacket) + sizeof(size_t)));
            return nullptr; // invalid packet
        }
        auto plannedArea = new PlannedAreaPacket(data);
        return std::unique_ptr<BasePacket>(plannedArea);
    }
    case PacketType::ROUTE_HISTORY: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid ROUTE_HISTORY packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(BasePacket) + sizeof(size_t)));
            return nullptr;
        }
        auto flightHistory = new FlightHistoryPacket(data);
        return std::unique_ptr<BasePacket>(flightHistory);
    }
    case PacketType::PLANNED_ROUTE: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid PLANNED_ROUTE packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(BasePacket) + sizeof(size_t)));
            return nullptr;
        }
        auto plannedRoute = new PlannedRoutePacket(data);
        return std::unique_ptr<BasePacket>(plannedRoute);
    }
    case PacketType::COMPONENT_STATUS: {
        if (len < sizeof(ComponentStatus)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid COMPONENT_STATUS packet size: %d, should be %d", len,
                     static_cast<int>(sizeof(ComponentStatus)));
            return nullptr; // invalid packet
        }
        auto status = new ComponentStatus(data);
        return std::unique_ptr<BasePacket>(status);
    }
    default:
        ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Unknown packet type: %02x", static_cast<int>(type));
        return nullptr;
    }
}

void Flight_Communication::sendPosition() {
    Coordinate position = GPS_Reader.getCurrentPosition();
    PositionUpdate packet = {
        GPS_Reader.getGPSLatestTime(),
        position
    };

    LoRa_Communication.sendData(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
}

void Flight_Communication::sendSensorUpdate() {
    float pressure = Barometer.getPressure();
    SensorUpdate packet = {
        GPS_Reader.getGPSLatestTime(),
        pressure,
    };

    LoRa_Communication.sendData(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
}

#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
void Flight_Communication::requestRouteHistory() {
    BasePacket packet = {
        GPS_Reader.getGPSLatestTime(),
        PacketType::ROUTE_HISTORY_REQUEST
    };

    LoRa_Communication.sendData(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
}

void Flight_Communication::sendPlannedArea(const std::vector<Coordinate>& shape) {
    BasePacket packet = {
        GPS_Reader.getGPSLatestTime(),
        PacketType::PLANNED_AREA
    };
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
    BasePacket packet = {
        GPS_Reader.getGPSLatestTime(),
        PacketType::ROUTE_HISTORY
    };
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
    BasePacket packet = {
        GPS_Reader.getGPSLatestTime(),
        PacketType::PLANNED_ROUTE
    };
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
    ComponentStatus status = {
    };
    status.timestamp = GPS_Reader.getGPSLatestTime();
    status.type = PacketType::COMPONENT_STATUS;
    status.gps = FlightStorage.getPlaneGPSConnectionState();
    // for simplicity, we assume other components are always connected in this example
    status.barometer = FlightStorage.getPlaneBarometerConnectionState();
    status.gyroscope = FlightStorage.getPlaneGyroscopeConnectionState();
    status.motorControl = FlightStorage.getPlaneMotorControlConnectionState();

    LoRa_Communication.sendData(reinterpret_cast<const uint8_t*>(&status), sizeof(ComponentStatus));
}
#endif
