#include "route_planner.hpp"

#include <cmath>

#include "geo_helper.hpp"

static RoutePlannerClass* route_planner;

RoutePlannerClass& RoutePlanner = RoutePlannerClass::getInstance();

RoutePlannerClass* RoutePlannerClass::getInstancePtr() {
    if (!route_planner) {
        route_planner = new RoutePlannerClass();
    }
    return route_planner;
}

RoutePlannerClass& RoutePlannerClass::getInstance() {
    return *getInstancePtr();
}


RoutePlannerClass::RoutePlannerClass() {}

Coordinate RoutePlannerClass::getIntersection(const std::vector<Coordinate>& shape, const Coordinate& origin,
                                              const Point2d& direction) {

    return {NAN, NAN};
}

std::vector<std::pair<Coordinate, Coordinate>> RoutePlannerClass::generateSimpleSweepLines(const std::vector<Coordinate>& shape, float maxDistance) {
    Coordinate leftMost{}, rightMost{}, topMost{}, bottomMost{};
    getMostOuterPoints(shape, leftMost, rightMost, topMost, bottomMost);
    Coordinate topLeft = {leftMost.longitude, topMost.latitude};
    Coordinate bottomRight = {rightMost.longitude, bottomMost.latitude};

    const float latDiff = topMost.latitude - bottomMost.latitude;
    const int lineCount = std::ceil(latitudeDiffToMeters(latDiff) / maxDistance);
    const float lineDistance = latDiff / (float)lineCount;

    std::vector<std::pair<Coordinate, Coordinate>> lines;
    for (int i = 0; i < lineCount; i++) {
        std::pair<Coordinate, Coordinate> line = {
            {topLeft.longitude, topLeft.latitude + i * lineDistance},
            {bottomRight.longitude, topLeft.latitude + i * lineDistance}
        };
        lines.push_back(line);
    }
    return lines;
}

Point2d RoutePlannerClass::intersection(const Point2d& a, const Point2d& b, const Point2d& c, const Point2d& d) {
    auto intersect = Line2d(a, b).intersects(Line2d(c, d));
    if (intersect()) {
        return intersect.get();
    }
    return {NAN, NAN};
}


void RoutePlannerClass::getMostOuterPoints(const std::vector<Coordinate>& shape, Coordinate& leftMost,
                                           Coordinate& rightMost, Coordinate& topMost, Coordinate& bottomMost) {
    if (shape.empty()) {
        leftMost = rightMost = topMost = bottomMost = {NAN, NAN};
        return;
    }

    leftMost = rightMost = topMost = bottomMost = shape[0];

    for (const auto& point : shape) {
        if (point.longitude < leftMost.longitude) {
            leftMost = point;
        }
        if (point.longitude > rightMost.longitude) {
            rightMost = point;
        }
        if (point.latitude > topMost.latitude) {
            topMost = point;
        }
        if (point.latitude < bottomMost.latitude) {
            bottomMost = point;
        }
    }
}
