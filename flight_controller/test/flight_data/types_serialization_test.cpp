#include "types.hpp"

#include <gtest/gtest.h>

TEST(CoordinateIsInvalidTest, zeroIsValid) {
    EXPECT_FALSE(Coordinate::isInvalid({.longitude = 0, .latitude = 0}));
}

TEST(CoordinateIsInvalidTest, boundaryValuesAreValid) {
    EXPECT_FALSE(Coordinate::isInvalid({.longitude = 180, .latitude = 90}));
    EXPECT_FALSE(Coordinate::isInvalid({.longitude = -180, .latitude = -90}));
}

TEST(CoordinateIsInvalidTest, outOfRangeLatitudeIsInvalid) {
    EXPECT_TRUE(Coordinate::isInvalid({.longitude = 0, .latitude = 91.1f}));
    EXPECT_TRUE(Coordinate::isInvalid({.longitude = 0, .latitude = -91.1f}));
}

TEST(CoordinateIsInvalidTest, outOfRangeLongitudeIsInvalid) {
    EXPECT_TRUE(Coordinate::isInvalid({.longitude = 181.1f, .latitude = 0}));
    EXPECT_TRUE(Coordinate::isInvalid({.longitude = -181.1f, .latitude = 0}));
}

TEST(CoordinateIsInvalidTest, sentinelInvalidValueIsInvalid) {
    EXPECT_TRUE(Coordinate::isInvalid(COORDINATE_INIT_INVALID()));
}

TEST(CoordinateEqualityTest, identicalCoordinatesAreEqual) {
    const Coordinate a{.longitude = 1.5f, .latitude = 2.5f};
    const Coordinate b{.longitude = 1.5f, .latitude = 2.5f};
    EXPECT_TRUE(a == b);
}

TEST(CoordinateEqualityTest, differingCoordinatesAreNotEqual) {
    const Coordinate a{.longitude = 1.5f, .latitude = 2.5f};
    const Coordinate b{.longitude = 1.5f, .latitude = 2.6f};
    EXPECT_FALSE(a == b);
}

TEST(CoordinateJsonTest, roundTripsThroughJson) {
    const Coordinate original{.longitude = 12.34f, .latitude = -56.78f};
    const nlohmann::json j = original;
    const Coordinate decoded = j.get<Coordinate>();
    EXPECT_TRUE(original == decoded);
}

TEST(CoordinateJsonTest, usesExpectedFieldNames) {
    const Coordinate original{.longitude = 1.0f, .latitude = 2.0f};
    const nlohmann::json j = original;
    EXPECT_TRUE(j.contains("longitude"));
    EXPECT_TRUE(j.contains("latitude"));
}

TEST(ConnectionStateJsonTest, roundTripsThroughJson) {
    for (const auto state : {ConnectionState::CONNECTING, ConnectionState::CONNECTED}) {
        const nlohmann::json j = state;
        EXPECT_EQ(j.get<ConnectionState>(), state);
    }
}

TEST(ConnectionStateJsonTest, usesExpectedStringValues) {
    EXPECT_EQ(nlohmann::json(ConnectionState::CONNECTING), "connecting");
    EXPECT_EQ(nlohmann::json(ConnectionState::CONNECTED), "connected");
}

TEST(FlightStateJsonTest, roundTripsThroughJson) {
    for (const auto state : {FlightState::PLANNING, FlightState::PLANNED, FlightState::FLYING, FlightState::RETURNING}) {
        const nlohmann::json j = state;
        EXPECT_EQ(j.get<FlightState>(), state);
    }
}

TEST(FlightStateJsonTest, usesExpectedStringValues) {
    EXPECT_EQ(nlohmann::json(FlightState::PLANNING), "planning");
    EXPECT_EQ(nlohmann::json(FlightState::PLANNED), "planned");
    EXPECT_EQ(nlohmann::json(FlightState::FLYING), "flying");
    EXPECT_EQ(nlohmann::json(FlightState::RETURNING), "returning");
}
