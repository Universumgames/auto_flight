#pragma once

#include <cstring>
#include <ctime>

#include "DeviceId.hpp"
#include "esp_log.h"
#include "../types.hpp"

typedef const uint8_t* RawSerializedPacket;
typedef std::pair<std::unique_ptr<uint8_t[]>, size_t> SerializedPacket;

#define SERIALIZE_THIS_PACKET() \
    nlohmann::json j = *this; \
    std::vector<uint8_t> buffer; \
    try{ \
        nlohmann::json::to_cbor(j, buffer); \
    } catch (const std::exception& e) { \
        ESP_LOGE("BasePacket", "Error serializing packet: %s", e.what()); \
    } \
    auto packet = std::make_unique<uint8_t[]>(buffer.size()); \
    std::memcpy(packet.get(), buffer.data(), buffer.size()); \
    return {std::move(packet), buffer.size()};

#define DESERIALIZE_TO_THIS_PACKET(packet, len) \
    try { \
        nlohmann::json j = nlohmann::json::from_cbor(packet, packet + len); \
        j.get_to(*this); \
    } catch (const std::exception& e) { \
        ESP_LOGE("BasePacket", "Error deserializing packet: %s", e.what()); \
    }

enum class PacketType: uint8_t {
    // sensors
    SENSOR_UPDATE = 0x10,
    POSITION = 0x11,
    COMPONENT_STATUS = 0x12,

    // flight data
    /// the planned route by the plane to cover a specified area
    PLANNED_ROUTE = 0x30,
    /// the area the plane has to cover
    PLANNED_AREA = 0x31,
    /// the complete history of the plane's route since takeoff
    ROUTE_HISTORY = 0x32,
    /// requesting the complete history of the planes route since takeoff, use with caution
    ROUTE_HISTORY_REQUEST = 0x33,
    /// confirmation of the planned route by the base station
    PLANNED_ROUTE_CONFIRMATION = 0x34
};

struct IBasePacket {
    virtual std::string toString() = 0;
    virtual ~IBasePacket() = default;
    [[nodiscard]] virtual SerializedPacket serialize() const = 0;
};

struct BasePacket : public IBasePacket {
    time_t timestamp{};
    PacketType type;
    uint32_t id{};

    std::string toString() override {
        return "BasePacket{timestamp=" + std::to_string(timestamp) + ", type=" + std::to_string(static_cast<int>(type))
            + ", id=" + std::to_string(id) + "}";
    }

    BasePacket(const BasePacket& packet) {
        this->timestamp = packet.timestamp;
        this->type = packet.type;
        this->id = packet.id;
    }

    BasePacket(RawSerializedPacket packet, size_t len) : timestamp(), type(PacketType::COMPONENT_STATUS) {
        /*printf("Deserializing packet of length %zu: ", len);
        for (size_t i = 0; i < len; i++) {
            printf("%02x ", packet[i]);
        }
        printf("\n");
        printf("\n");
        printf("\n");*/
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    BasePacket(time_t time = 0, PacketType type = PacketType::COMPONENT_STATUS) : timestamp(time), type(type),
        id(DeviceId::get32()) {}

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BasePacket, timestamp, type, id)

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

private:
    BasePacket(time_t time, PacketType type, uint32_t id) : timestamp(time), type(type), id(id) {}
public:
    static BasePacket nullPacket(uint32_t id, PacketType type, time_t time = 0) {
        return {time, type, id};
    }
};
