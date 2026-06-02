#pragma once
#include "LoRa_Communication.hpp"
#include "Packets.hpp"

using FlightPacket = std::variant<BasePacket, SensorUpdate, PositionUpdate>;

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

class Flight_Communication {
private:
public:
    static void begin();

    static void sendSensorUpdate();

    static void sendPosition();

#ifdef FLIGHT_DEVICE_TYPE_PLANE
    static void sendPlannedRoute();

    static void sendRouteHistory();

    static void sendComponentStatus();
#endif

#ifdef FLIGHT_DEVICE_TYPE_BASE_STATION
    static void requestRouteHistory();

    static void sendPlannedArea(std::vector<Coordinate> shape);
#endif

    static std::unique_ptr<FlightPacket> decodePacket(const uint8_t* data, std::size_t len);
    static std::unique_ptr<FlightPacket> decodePacket(const LoRaPacket& packet);
};
