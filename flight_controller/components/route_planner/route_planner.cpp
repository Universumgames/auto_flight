#include "route_planner.hpp"

#include "BoustrophedonRoutePlanner.hpp"
#include "esp_log.h"
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

RouteData RoutePlannerClass::planRoute(const std::vector<Coordinate>& shape, RouteAlgorithm algorithm, float maxPointDistance, float maxSwathWidth, int overlapPercent) {
    for (auto& impl : implementations) {
        if (impl->supportsAlgorithm(algorithm)) {
            auto rawPath = impl->planRoute(shape, (float)overlapPercent / 100.0f);
            if (rawPath.empty()) {
                ESP_LOGW("RoutePlanner", "No raw path generated for the given shape, cannot plan route");
                return {};
            }
            auto interpolatedPath = IRoutePlanner::calculateIntermediatePoints(rawPath, maxPointDistance);
            return {
                interpolatedPath,
                rawPath,
                algorithm,
                overlapPercent
            };
        }
    }
    return {};
}
