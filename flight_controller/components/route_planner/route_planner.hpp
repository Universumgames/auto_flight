#pragma once
#include "./route.hpp"
#include "homog2d.hpp"

using namespace h2d;

class RoutePlannerClass {
private:
    RoutePlannerClass();
    ~RoutePlannerClass() = delete;
public:
    static RoutePlannerClass *getInstancePtr();
    static RoutePlannerClass& getInstance();

public:
    /**
     * Plan a waypoint mission to cover the given shape. The shape is defined as a list of coordinates (longitude, latitude) that form a closed polygon. The returned route is a list of waypoints that the drone should follow to cover the area defined by the shape.
     * @param shape coordinate array defining shape to cover
     * @param maxPointDistance maximum distance between points on calculated path
     * @param maxSwathWidth the maximum distance between two paths
     * @param overlapFactor the percentage of overlap between two paths (0.0 - 1.0)
     * @return list of points to cover the given shape
     */
    std::vector<Coordinate> planRoute(const std::vector<Coordinate>& shape, float maxPointDistance, float maxSwathWidth, float overlapFactor);

public:
    /**
     * Get lines that cover shape
     * @param shape coordinate array to define shape
     * @param maxDistance distance between lines in degree
     * @return list of lines that cover the shape, each line is defined by two coordinates (start and end)
     */
    static std::vector<std::pair<Coordinate, Coordinate>> generateSimpleSweepLines(const std::vector<Coordinate>& shape, float maxDistance);

    static Coordinate getIntersection(const std::vector<Coordinate>& shape, const Coordinate& origin, const Point2d& direction);

    /**
     * Get the point of intersection between the lines ab and cd
     * @param a point a
     * @param b point b
     * @param c point c
     * @param d point d
     * @return intersection point or (NAN, NAN) if lines are parallel or do not intersect within the line segments
     */
    static Point2d intersection(const Point2d& a, const Point2d& b, const Point2d& c, const Point2d& d);

    static void getMostOuterPoints(const std::vector<Coordinate>& shape, Coordinate& leftMost, Coordinate& rightMost, Coordinate& topMost, Coordinate& bottomMost);

};

extern RoutePlannerClass& RoutePlanner;