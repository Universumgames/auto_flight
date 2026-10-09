#include "Flight_Communication.hpp"

#include <iostream>

// Pure packet decode logic, split out of Flight_Communication.cpp so it can
// be compiled and unit tested natively - it touches no hardware, only the
// wire format defined in packets/*.hpp.

constexpr const char* TAG_FLIGHT_COMMUNICATION = "Flight_Communication";

std::unique_ptr<BasePacket> Flight_Communication::decodePacket(const LoRaPacket& packet) {
    return decodePacket(packet.payload, packet.length);
}

// Function-try-block: packets are CBOR encoded, so malformed or truncated
// payloads make nlohmann throw - reject them instead of crashing.
std::unique_ptr<BasePacket> Flight_Communication::decodePacket(const uint8_t* data, std::size_t len) try {
    if (len < sizeof(BasePacket)) {
        ESP_LOGE(TAG_FLIGHT_COMMUNICATION, "Received packet too small: %d bytes, should be at least %d bytes", len,
                 static_cast<int>(sizeof(BasePacket)));
        return nullptr; // invalid packet
    }

    auto basePacket = BasePacket(data, len);

    switch (auto type = basePacket.type) {
    case PacketType::SENSOR_UPDATE: {
        if (len < sizeof(SensorUpdate)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid SENSOR_UPDATE packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(SensorUpdate)));
            return nullptr; // invalid packet
        }
        auto sensorUpdate = new SensorUpdate(data, len);

        return std::unique_ptr<BasePacket>(sensorUpdate);
    }
    case PacketType::POSITION: {
        if (len < sizeof(PositionUpdate)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid POSITION packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(PositionUpdate)));
            return nullptr; // invalid packet
        }
        auto positionUpdate = new PositionUpdate(data, len);
        return std::unique_ptr<BasePacket>(positionUpdate);
    }
    case PacketType::ROUTE_HISTORY_REQUEST: {
        auto routeHistoryRequest = new BasePacket(data, len);
        return std::unique_ptr<BasePacket>(routeHistoryRequest);
    }
    case PacketType::PLANNED_AREA: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid PLANNED_AREA packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(BasePacket) + sizeof(size_t)));
            return nullptr; // invalid packet
        }
        auto plannedArea = new PlannedAreaPacket(data, len);
        return std::unique_ptr<BasePacket>(plannedArea);
    }
    case PacketType::ROUTE_HISTORY: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid ROUTE_HISTORY packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(BasePacket) + sizeof(size_t)));
            return nullptr;
        }
        auto flightHistory = new FlightHistoryPacket(data, len);
        return std::unique_ptr<BasePacket>(flightHistory);
    }
    case PacketType::PLANNED_ROUTE: {
        if (len < sizeof(BasePacket) + sizeof(size_t)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid PLANNED_ROUTE packet size: %d, should be at least %d", len,
                     static_cast<int>(sizeof(BasePacket) + sizeof(size_t)));
            return nullptr;
        }
        auto plannedRoute = new PlannedRoutePacket(data, len);
        return std::unique_ptr<BasePacket>(plannedRoute);
    }
    case PacketType::PLANNED_ROUTE_CONFIRMATION: {
        if (len < sizeof(PlannedRouteConfirmationPacket)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid PLANNED_ROUTE_CONFIRMATION packet size: %d, should be %d", len,
                     static_cast<int>(sizeof(PlannedRouteConfirmationPacket)));
            return nullptr; // invalid packet
        }
        auto confirmation = new PlannedRouteConfirmationPacket(data, len);
        return std::unique_ptr<BasePacket>(confirmation);
    }
    case PacketType::COMPONENT_STATUS: {
        if (len < sizeof(ComponentStatus)) {
            ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Invalid COMPONENT_STATUS packet size: %d, should be %d", len,
                     static_cast<int>(sizeof(ComponentStatus)));
            return nullptr; // invalid packet
        }
        auto status = new ComponentStatus(data, len);
        return std::unique_ptr<BasePacket>(status);
    }
    default:
        ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Unknown packet type: %02x", static_cast<int>(type));
        for (std::size_t i = 0; i < len; i++) {
            std::cout << std::hex << data[i];
        }
        std::cout << std::dec << std::endl;
        return nullptr;
    }
} catch (const nlohmann::json::exception& e) {
    ESP_LOGW(TAG_FLIGHT_COMMUNICATION, "Failed to decode packet of %d bytes: %s", static_cast<int>(len), e.what());
    return nullptr;
}
