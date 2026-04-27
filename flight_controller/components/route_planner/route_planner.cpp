//
// Created by Tom Arlt on 27.04.26.
//

#include "route_planner.hpp"

static RoutePlannerClass* route_planner;

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
                                              const Vector2D& direction) {}

Vector2D RoutePlannerClass::intersection(Vector2D a, Vector2D b, Vector2D c, Vector2D d) {
    if ((a.x == b.x && a.y == b.y) || (c.x == d.x && c.y == d.y)) {
        return {NAN, NAN}; // Lines are points
    }

    float denominator = ((d.y - c.y) * (b.x - a.x) - (d.x - c.x) * (b.y - a.y));
    if (denominator == 0) {
        return {NAN, NAN}; // Lines are parallel
    }

    float ua = ((d.x - c.x) * (a.y - c.y) - (d.y - c.y) * (a.x - c.x)) / denominator;
    float ub = ((b.x - a.x) * (a.y - c.y) - (b.y - a.y) * (a.x - c.x)) / denominator;

    if (ua < 0 || ub < 0 || ua > 1 || ub > 1) {
        return {NAN, NAN};
    }

    float x = a.x + ua * (b.x - a.x);
    float y = a.y + ub * (b.y - a.y);
    return {x, y};
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
