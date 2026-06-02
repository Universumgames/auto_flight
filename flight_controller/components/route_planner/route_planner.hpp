#pragma once
#include "homog2d.hpp"
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
     * @param maxPointDistance maximum distance between points on calculated path (in m)
     * @param maxSwathWidth the maximum distance between two paths (in degree)
     * @param overlapFactor the percentage of overlap between two paths (0.0 - 0.9)
     * @return list of points to cover the given shape
     */
    std::vector<Coordinate> planRoute(const std::vector<Coordinate>& shape, float maxPointDistance, float maxSwathWidth, float overlapFactor);

public:
    /**
     * Get lines that cover shape
     * horizontal lines parallel to equator
     * treats shape as rectangle
     * @param shape coordinate array to define closed shape
     * @param swathDistance coverage of lines in degrees
     * @return list of lines that cover the shape, each line is defined by two coordinates (start and end)
     */
    static std::vector<std::pair<Coordinate, Coordinate>> generateSimpleSweepLines(const std::vector<Coordinate>& shape, float swathDistance);

    /**
     * Get lines that cover shape
     * horizontal lines parallel to equator
     * @param shape coordinate array to define closed shape
     * @param swathDistance coverage of lines in degrees
     * @return list of lines that cover the shape, each line is defined by start and end point
     */
    static std::vector<std::pair<Coordinate, Coordinate>> generateContainedSimpleSweepLines(const std::vector<Coordinate>& shape, float swathDistance);

    static std::vector<Coordinate> generateSimpleSweepPath(const std::vector<Coordinate>& shape, float swathDistance);

    static std::pair<Coordinate, Coordinate> getShapeIntersection(const std::vector<Coordinate>& shape, const Coordinate& p1, const Coordinate& p2);

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

    /**
     * Check if the shape is crossing the anti meridian line (180th meridian)
     * @param shape the shape to check
     * @return true if the shape is crossing the anti meridian line, false otherwise
     */
    static bool isCrossingAntiMeridian(const std::vector<Coordinate>& shape);

};

extern RoutePlannerClass& RoutePlanner;