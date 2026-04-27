#pragma once
#include "./route.hpp"

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

private:
    /**
     * Get lines that cover shape
     * @param shape coordinate array to define shape
     * @param maxDistance distance between lines
     * @return list of lines that cover the shape, each line is defined by two coordinates (start and end)
     */
    std::vector<std::pair<Coordinate, Coordinate>> generateSimpleSweepLines(const std::vector<Coordinate>& shape, float maxDistance);

    Coordinate getIntersection(const std::vector<Coordinate>& shape, const Coordinate& origin, const Vector2D& direction);

    /**
     * Get the point of intersection between the lines ab and cd
     * @param a point a
     * @param b point b
     * @param c point c
     * @param d point d
     * @return intersection point or (NAN, NAN) if lines are parallel or do not intersect within the line segments
     */
    Vector2D intersection(Vector2D a, Vector2D b, Vector2D c, Vector2D d);

    void getMostOuterPoints(const std::vector<Coordinate>& shape, Coordinate& leftMost, Coordinate& rightMost, Coordinate& topMost, Coordinate& bottomMost);



};

extern RoutePlannerClass& RoutePlanner;