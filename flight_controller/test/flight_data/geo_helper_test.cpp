#include "geo_helper.hpp"

#include <gtest/gtest.h>

TEST(DistanceInMetersTest, samePointIsZero) {
    const Coordinate a{.longitude = 12.3f, .latitude = 45.6f};
    EXPECT_NEAR(distanceInMeters(a, a), 0.0f, 0.001f);
}

TEST(DistanceInMetersTest, oneDegreeLatitudeIsApproximately111km) {
    const Coordinate a{.longitude = 0, .latitude = 0};
    const Coordinate b{.longitude = 0, .latitude = 1};
    EXPECT_NEAR(distanceInMeters(a, b), 111320.0f, 200.0f);
}

TEST(DistanceInMetersTest, oneDegreeLongitudeAtEquatorMatchesLatitude) {
    const Coordinate a{.longitude = 0, .latitude = 0};
    const Coordinate lonB{.longitude = 1, .latitude = 0};
    const Coordinate latB{.longitude = 0, .latitude = 1};
    EXPECT_NEAR(distanceInMeters(a, lonB), distanceInMeters(a, latB), 50.0f);
}

TEST(DistanceInMetersTest, longitudeDegreeShrinksAwayFromEquator) {
    const Coordinate equatorA{.longitude = 0, .latitude = 0};
    const Coordinate equatorB{.longitude = 1, .latitude = 0};

    const Coordinate highLatA{.longitude = 0, .latitude = 60};
    const Coordinate highLatB{.longitude = 1, .latitude = 60};

    EXPECT_LT(distanceInMeters(highLatA, highLatB), distanceInMeters(equatorA, equatorB));
}

TEST(LatitudeDiffToMetersTest, matchesDistanceInMeters) {
    const Coordinate a{.longitude = 0, .latitude = 0};
    const Coordinate b{.longitude = 0, .latitude = 2};
    EXPECT_NEAR(latitudeDiffToMeters(2.0f), distanceInMeters(a, b), 0.5f);
}

TEST(LongitudeDiffToMetersTest, matchesDistanceInMetersAtGivenLatitude) {
    const Coordinate a{.longitude = 0, .latitude = 45};
    const Coordinate b{.longitude = 3, .latitude = 45};
    EXPECT_NEAR(longitudeDiffToMeters(3.0f, 45.0f), distanceInMeters(a, b), 0.5f);
}

TEST(MetersToLatitudeDegreeTest, roundTripsWithLatitudeDiffToMeters) {
    const float degrees = 1.0f;
    const float meters = latitudeDiffToMeters(degrees);
    EXPECT_NEAR(metersToLatitudeDegree(meters), degrees, 0.001f);
}

TEST(MetersToLatitudeDegreeTest, zeroMetersIsZeroDegrees) {
    EXPECT_FLOAT_EQ(metersToLatitudeDegree(0.0f), 0.0f);
}
