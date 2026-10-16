#pragma once
#include "base.hpp"

struct DataRequest: public BasePacket {
    PacketType requestedType;

    std::string toString() override {
        return "DataRequest{base=" + BasePacket::toString() + ", requestedType=" + std::to_string(static_cast<int>(requestedType)) + "}";
    }

    DataRequest(const DataRequest& packet) : BasePacket(packet) {
        this->requestedType = packet.requestedType;
    }

    DataRequest(RawSerializedPacket packet, size_t len) : BasePacket() {
        DESERIALIZE_TO_THIS_PACKET(packet, len);
    }

    DataRequest(time_t time = 0, PacketType requestedType = PacketType::SENSOR_UPDATE) : BasePacket(time, PacketType::DATA_REQUEST),
                                                                                       requestedType(requestedType) {}

    [[nodiscard]] SerializedPacket serialize() const override {
        SERIALIZE_THIS_PACKET();
    }

    NLOHMANN_DEFINE_DERIVED_TYPE_INTRUSIVE(DataRequest, BasePacket, requestedType)

};
