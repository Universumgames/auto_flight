#pragma once
#include "homog2d.hpp"
#include "types.hpp"

using namespace h2d;

namespace GeometryHelper{
    std::pair<Coordinate, Coordinate> getShapeIntersection(const std::vector<Coordinate>& shape, const Coordinate& p1, const Coordinate& p2);

    /**
     * Get the point of intersection between the lines ab and cd
     * @param a point a
     * @param b point b
     * @param c point c
     * @param d point d
     * @return intersection point or (NAN, NAN) if lines are parallel or do not intersect within the line segments
     */
    Point2d intersection(const Point2d& a, const Point2d& b, const Point2d& c, const Point2d& d);

    void getMostOuterPoints(const std::vector<Coordinate>& shape, Coordinate& leftMost, Coordinate& rightMost, Coordinate& topMost, Coordinate& bottomMost);

    /**
     * Check if the shape is crossing the anti meridian line (180th meridian)
     * @param shape the shape to check
     * @return true if the shape is crossing the anti meridian line, false otherwise
     */
    bool isCrossingAntiMeridian(const std::vector<Coordinate>& shape);
}
