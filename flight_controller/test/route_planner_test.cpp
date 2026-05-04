#include "route_planner.hpp"

#include <gtest/gtest.h>

#include "route.hpp"
#include "gmock/gmock-matchers.h"

MATCHER_P2(CoordinateEq, expected, actual, "") {
    return expected.latitude == actual.latitude && expected.longitude == actual.longitude;
}

TEST(RoutePlannerTest, mostOuterPoints) {
    std::vector<Coordinate> shape = {
        {0, 50},
        {-10, 3},
        {20, -50},
        {50, 10},
        {20, 20}
    };
    Coordinate leftMost{}, rightMost{}, topMost{}, bottomMost{};
    RoutePlanner.getMostOuterPoints(shape, leftMost, rightMost, topMost, bottomMost);

    CoordinateEq(topMost, (Coordinate{0, 50}));
    CoordinateEq(bottomMost, (Coordinate{-10, 3}));
    CoordinateEq(leftMost, (Coordinate{20, -50}));
    CoordinateEq(rightMost, (Coordinate{50, 10}));
}

TEST(RoutePlannerTest, intersection) {
    Coordinate p1 = {5,0};
    Coordinate p2 = {0,0};
    std::vector<Coordinate> shape = {p1, p2};

    Coordinate intersection = RoutePlanner.getIntersection(
        shape,
        {2,-5},
        {0,1});

    CoordinateEq(intersection, (Coordinate{2, 0}));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    // if you plan to use GMock, replace the line above with
    // ::testing::InitGoogleMock(&argc, argv);

    if (RUN_ALL_TESTS())
        ;

    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}
