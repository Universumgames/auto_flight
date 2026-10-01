#pragma once
#include <cstdint>
#include <memory>

#include "LoRaPacket.hpp"
#include "Packets.hpp"

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
    static void requestRouteHistory(uint32_t sourceId);

    static void sendPlannedArea(uint32_t destId, const AreaData& areaData);
#endif

    static std::unique_ptr<BasePacket> decodePacket(const uint8_t* data, std::size_t len);
    static std::unique_ptr<BasePacket> decodePacket(const LoRaPacket& packet);

//private:
    /**
     * @brief Send a packet over LoRa communication.
     * @param packet The packet to send.
     * @return true if the packet was sent successfully, false otherwise.
     */
    static bool sendPacket(const BasePacket& packet);

};
