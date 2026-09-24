#include "route_planner.hpp"

#include "BoustrophedonRoutePlanner.hpp"
#include "types.hpp"

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

RoutePlannerClass::RoutePlannerClass() {
    implementations = {
        new BoustrophedonRoutePlanner()
    };
}

std::vector<Coordinate> RoutePlannerClass::planRoute(const std::vector<Coordinate>& shape, RouteAlgorithm algorithm, float maxPointDistance, float maxSwathWidth, float overlapFactor) {
    for (auto& impl : implementations) {
        if (impl->supportsAlgorithm(algorithm)) {
            return IRoutePlanner::calculateIntermediatePoints(impl->planRoute(shape, overlapFactor), maxPointDistance);
        }
    }
    return {};
}
