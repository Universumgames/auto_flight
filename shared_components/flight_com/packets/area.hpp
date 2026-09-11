#pragma once

#include "base.hpp"

struct PlannedAreaPacket : public BasePacket {
    // size_t length; // hidden field in serialized data
    std::vector<Coordinate> shape;

    std::string toString() override {
        return "PlannedAreaPacket{base=" + BasePacket::toString() + ", shapeSize=" + std::to_string(shape.size()) + "}";
    }

    PlannedAreaPacket(const PlannedAreaPacket& packet) : BasePacket(packet) {
        this->shape = std::vector(packet.shape);
    }

    PlannedAreaPacket(RawSerializedPacket packet) : BasePacket(packet) {
        size_t dataOffset = sizeof(BasePacket);
        size_t length = 0;
        std::memcpy(&length, packet + dataOffset, sizeof(size_t));
        if (length > 255 || length < 1) {
            ESP_LOGE("PlannedAreaPacket", "Invalid planned area packet with length %u", length);
            return;
        }
        shape = std::vector<Coordinate>(length);
        dataOffset += sizeof(size_t);
        memcpy(shape.data(), packet + dataOffset, length * sizeof(Coordinate));
    }

    PlannedAreaPacket(time_t time = 0, std::vector<Coordinate> shape = {}) : BasePacket(time, PacketType::PLANNED_AREA),
                                                                             shape(shape) {}

    std::pair<std::unique_ptr<uint8_t[]>, size_t> serialize() const override {
        BasePacket packet = {
            timestamp,
            type
        };
        size_t length = shape.size();
        size_t dataSize = sizeof(BasePacket) + sizeof(size_t) + (shape.size() * sizeof(Coordinate));
        auto data = std::make_unique<uint8_t[]>(dataSize);
        uint8_t dataOffset = 0;
        std::memcpy(data.get(), &packet, sizeof(BasePacket));
        dataOffset += sizeof(BasePacket);
        std::memcpy(data.get() + dataOffset, &length, sizeof(size_t));
        dataOffset += sizeof(size_t);
        std::memcpy(data.get() + dataOffset, shape.data(), sizeof(Coordinate) * shape.size());
        return {std::move(data), dataSize};
    }
};
