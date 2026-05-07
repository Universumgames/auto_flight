#include "route_planner.hpp"

#include <gtest/gtest.h>

#include "route.hpp"
#include "serializer.hpp"
#include "gmock/gmock-matchers.h"

#define CoordinateEq(expected, actual) { \
    EXPECT_EQ(expected.latitude, actual.latitude); \
    EXPECT_EQ(expected.longitude, actual.longitude);\
}

#define EXPECT_IN_BETWEEN(actual, lower, higher) { \
    EXPECT_LE(lower, actual); \
    EXPECT_GE(higher, actual); \
}

TEST(RoutePlannerTest, mostOuterPoints) {
    const std::vector<Coordinate> shape = {
        {0, 50},
        {-10, 3},
        {20, -50},
        {50, 10},
        {20, 20}
    };
    Coordinate leftMost{}, rightMost{}, topMost{}, bottomMost{};
    RoutePlannerClass::getMostOuterPoints(shape, leftMost, rightMost, topMost, bottomMost);

    CoordinateEq(topMost, (Coordinate{0, 50}));
    CoordinateEq(bottomMost, (Coordinate{20, -50}));
    CoordinateEq(leftMost, (Coordinate{-10, 3}));
    CoordinateEq(rightMost, (Coordinate{50, 10}));
}

TEST(RoutePlannerTest, intersection) {
    Coordinate p1 = {0,5};
    Coordinate p2 = {0,-5};
    const std::vector<Coordinate> shape = {p1, p2};

    auto intersections = RoutePlannerClass::getShapeIntersection(
        shape,
        {-1,0},
        {1,0});

    CoordinateEq(intersections.first, (Coordinate{0, 0}));
}

TEST(RoutePlannerTest, simpleSweepLines) {
    const std::vector<Coordinate> shape = {
        {0,0},
        {0,10},
        {10,10},
        {10,0}
    };
    constexpr float swathDistance = 2;
    auto simpleSweepLines = RoutePlannerClass::generateSimpleSweepLines(shape, swathDistance);

    EXPECT_EQ(simpleSweepLines.size(), 5);
    std::pair<Coordinate, Coordinate> lastLine = {{0, 10}, {10, 10}};
    std::vector<Coordinate> path = {};
    for (const auto& line : simpleSweepLines) {
        std::cerr << line.first.latitude << ", " << line.first.longitude << "\t" << line.second.latitude << ", " << line.second.longitude << std::endl;
        EXPECT_LT(abs(line.first.latitude - lastLine.first.latitude), swathDistance);
        EXPECT_EQ(line.first.latitude, line.second.latitude);
        EXPECT_IN_BETWEEN(line.first.latitude, 0, 10);
        EXPECT_IN_BETWEEN(line.first.longitude, 0, 10);
        EXPECT_IN_BETWEEN(line.second.longitude, 0, 10);
        EXPECT_IN_BETWEEN(line.second.latitude, 0, 10);
        lastLine = line;
        path.push_back(line.first);
        path.push_back(line.second);
    }

    std::ofstream os("sweepLines.txt");
    serialize(path, os);
    os.close();
}

TEST(RoutePlannerTest, sweepPath) {
    const std::vector<Coordinate> shape = {
        {0,0},
        {0,10},
        {10,10},
        {10,0}
    };
    constexpr float swathDistance = 2;
    auto path = RoutePlannerClass::generateSimpleSweepPath(shape, swathDistance);

    float lastLat = 300;
    for (const auto& waypoint : path) {
        EXPECT_LE(waypoint.latitude, lastLat + 0.001);
        lastLat = waypoint.latitude;
    }

    std::ofstream os("sweepPath.txt");
    serialize(path, os);
    os.close();
}