#pragma once
#include "homog2d.hpp"
#include "IRoutePlanner.hpp"
#include "RouteData.hpp"
#include "types.hpp"

using namespace h2d;

class RoutePlannerClass {
private:
    RoutePlannerClass();
public:
    ~RoutePlannerClass() = delete;
public:
    static RoutePlannerClass *getInstancePtr();
    static RoutePlannerClass& getInstance();

public:
    /**
     * Plan a waypoint mission to cover the given shape. The shape is defined as a list of coordinates (longitude, latitude) that form a closed polygon. The returned route is a list of waypoints that the drone should follow to cover the area defined by the shape.
     * @param shape coordinate array defining shape to cover
     * @param algorithm the route algorithm to use
     * @param maxPointDistance maximum distance between points on calculated path (in m)
     * @param maxSwathWidth the maximum distance between two paths (in degree)
     * @param overlapPercent the percentage of overlap between two paths (0 - 99)
     * @return list of points to cover the given shape
     */
    RouteData planRoute(const std::vector<Coordinate>& shape, RouteAlgorithm algorithm, float maxPointDistance, float maxSwathWidth, int overlapPercent);

private:
    std::vector<IRoutePlanner*> implementations;

};

extern RoutePlannerClass& RoutePlanner;