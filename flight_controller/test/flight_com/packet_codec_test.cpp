#include "Flight_Communication.hpp"

#include <gtest/gtest.h>

namespace {
    // Decodes whatever `packet.serialize()` produced, verifying the codec
    // round-trips through the exact byte buffer sendPacket() would emit.
    template <typename PacketT>
    std::unique_ptr<BasePacket> roundTrip(const PacketT& packet) {
        const auto [data, len] = packet.serialize();
        return Flight_Communication::decodePacket(data.get(), len);
    }
}

TEST(PacketCodecTest, sensorUpdateRoundTrips) {
    const SensorUpdate original(1234, 987.6f, 120.5f, 42);
    const auto decoded = roundTrip(original);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded->type, PacketType::SENSOR_UPDATE);
    const auto* sensorUpdate = dynamic_cast<SensorUpdate*>(decoded.get());
    ASSERT_NE(sensorUpdate, nullptr);
    EXPECT_EQ(sensorUpdate->timestamp, 1234);
    EXPECT_FLOAT_EQ(sensorUpdate->pressure, 987.6f);
    EXPECT_FLOAT_EQ(sensorUpdate->altitude, 120.5f);
    EXPECT_EQ(sensorUpdate->heading, 42);
}

TEST(PacketCodecTest, positionUpdateRoundTrips) {
    const Coordinate position{.longitude = 12.5f, .latitude = -33.25f};
    const PositionUpdate original(5678, position);
    const auto decoded = roundTrip(original);
    ASSERT_NE(decoded, nullptr);
    const auto* positionUpdate = dynamic_cast<PositionUpdate*>(decoded.get());
    ASSERT_NE(positionUpdate, nullptr);
    EXPECT_EQ(positionUpdate->timestamp, 5678);
    EXPECT_TRUE(positionUpdate->position == position);
}

TEST(PacketCodecTest, routeHistoryRequestRoundTrips) {
    const BasePacket original(111, PacketType::ROUTE_HISTORY_REQUEST);
    const auto decoded = roundTrip(original);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->type, PacketType::ROUTE_HISTORY_REQUEST);
    EXPECT_EQ(decoded->timestamp, 111);
}

TEST(PacketCodecTest, plannedAreaRoundTrips) {
    const std::vector<Coordinate> shape = {
        {.longitude = 0, .latitude = 0},
        {.longitude = 1, .latitude = 0},
        {.longitude = 1, .latitude = 1},
    };
    const PlannedAreaPacket original(222, shape);
    const auto decoded = roundTrip(original);
    ASSERT_NE(decoded, nullptr);
    const auto* plannedArea = dynamic_cast<PlannedAreaPacket*>(decoded.get());
    ASSERT_NE(plannedArea, nullptr);
    ASSERT_EQ(plannedArea->shape.size(), shape.size());
    for (size_t i = 0; i < shape.size(); i++) {
        EXPECT_TRUE(plannedArea->shape[i] == shape[i]);
    }
}

TEST(PacketCodecTest, flightHistoryRoundTrips) {
    const std::vector<Coordinate> history = {
        {.longitude = 5, .latitude = 5},
        {.longitude = 6, .latitude = 6},
    };
    const FlightHistoryPacket original(333, history);
    const auto decoded = roundTrip(original);
    ASSERT_NE(decoded, nullptr);
    const auto* flightHistory = dynamic_cast<FlightHistoryPacket*>(decoded.get());
    ASSERT_NE(flightHistory, nullptr);
    ASSERT_EQ(flightHistory->history.size(), history.size());
    for (size_t i = 0; i < history.size(); i++) {
        EXPECT_TRUE(flightHistory->history[i] == history[i]);
    }
}

TEST(PacketCodecTest, plannedRouteRoundTrips) {
    const std::vector<Coordinate> route = {
        {.longitude = -1, .latitude = -1},
    };
    const PlannedRoutePacket original(444, route);
    const auto decoded = roundTrip(original);
    ASSERT_NE(decoded, nullptr);
    const auto* plannedRoute = dynamic_cast<PlannedRoutePacket*>(decoded.get());
    ASSERT_NE(plannedRoute, nullptr);
    ASSERT_EQ(plannedRoute->route.size(), route.size());
    EXPECT_TRUE(plannedRoute->route[0] == route[0]);
}

TEST(PacketCodecTest, componentStatusRoundTrips) {
    const ComponentStatus original(555, ConnectionState::CONNECTED, ConnectionState::CONNECTING,
                                    ConnectionState::CONNECTED, ConnectionState::CONNECTING,
                                    ConnectionState::CONNECTED, true, FlightState::FLYING);
    const auto decoded = roundTrip(original);
    ASSERT_NE(decoded, nullptr);
    const auto* status = dynamic_cast<ComponentStatus*>(decoded.get());
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->gps, ConnectionState::CONNECTED);
    EXPECT_EQ(status->barometer, ConnectionState::CONNECTING);
    EXPECT_EQ(status->motorControl, ConnectionState::CONNECTED);
    EXPECT_EQ(status->magnetometer, ConnectionState::CONNECTING);
    EXPECT_EQ(status->accelerometer, ConnectionState::CONNECTED);
    EXPECT_TRUE(status->manualOverride);
    EXPECT_EQ(status->flightState, FlightState::FLYING);
}

TEST(PacketCodecTest, rejectsBufferSmallerThanBasePacket) {
    const uint8_t tooSmall[2] = {0, 0};
    const auto decoded = Flight_Communication::decodePacket(tooSmall, sizeof(tooSmall));
    EXPECT_EQ(decoded, nullptr);
}

TEST(PacketCodecTest, rejectsTruncatedSensorUpdate) {
    const SensorUpdate original(1, 2.0f, 3);
    const auto [data, len] = original.serialize();
    // Truncate to just the BasePacket header - too short for a full SensorUpdate.
    const auto decoded = Flight_Communication::decodePacket(data.get(), sizeof(BasePacket));
    EXPECT_EQ(decoded, nullptr);
}

TEST(PacketCodecTest, rejectsTruncatedPlannedArea) {
    const PlannedAreaPacket original(1, {{.longitude = 0, .latitude = 0}});
    const auto [data, len] = original.serialize();
    // Truncate below "BasePacket + length prefix" - too short to even read the count.
    const auto decoded = Flight_Communication::decodePacket(data.get(), sizeof(BasePacket));
    EXPECT_EQ(decoded, nullptr);
}

TEST(PacketCodecTest, rejectsUnknownPacketType) {
    BasePacket unknown(999, static_cast<PacketType>(0xEE));
    const auto [data, len] = unknown.serialize();
    const auto decoded = Flight_Communication::decodePacket(data.get(), len);
    EXPECT_EQ(decoded, nullptr);
}

TEST(PacketCodecTest, loRaPacketOverloadDelegatesToBufferOverload) {
    const SensorUpdate original(7, 1.5f, 9);
    auto [data, len] = original.serialize();
    LoRaPacket loRaPacket{len, data.get()};
    const auto decoded = Flight_Communication::decodePacket(loRaPacket);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->type, PacketType::SENSOR_UPDATE);
}
