#include "geometry_helper.hpp"

std::pair<Coordinate, Coordinate> GeometryHelper::getShapeIntersection(
    const std::vector<Coordinate>& shape, const Coordinate& p1, const Coordinate& p2) {
    std::vector<Point2d> shapePoints;
    bool wrapsOverAntiMeridian = isCrossingAntiMeridian(shape);
    std::ranges::transform(shape, std::back_inserter(shapePoints),
                           [wrapsOverAntiMeridian](const Coordinate& coord) {
                               if (wrapsOverAntiMeridian) {
                                   return Point2d(coord.longitude < 0 ? coord.longitude + 360 : coord.longitude,
                                                  coord.latitude);
                               }
                               return Point2d(coord.longitude, coord.latitude);
                           });
    CPolyline shapeLine(shapePoints);
    auto intersects = shapeLine.intersects(Line2d(Point2d{p1.longitude, p1.latitude}, Point2d{p2.longitude, p2.latitude}));

    if (intersects()) {
        auto intersections = intersects.get();
        // get most outer intersections to work for concave shapes
        Point2d leftMost{500, NAN}, rightMost{-500, NAN};
        for (const auto& point : intersections) {
            if (point.getX() < leftMost.getX()) {
                leftMost = point;
            }
            if (point.getX() > rightMost.getX()) {
                rightMost = point;
            }
        }
        return {
            {(float)leftMost.getX(), (float)leftMost.getY()},
            {(float)rightMost.getX(), (float)rightMost.getY()}
        };
    }
    return {{NAN, NAN}, {NAN, NAN}};
}

Point2d GeometryHelper::intersection(const Point2d& a, const Point2d& b, const Point2d& c, const Point2d& d) {
    auto intersect = Line2d(a, b).intersects(Line2d(c, d));
    if (intersect()) {
        return intersect.get();
    }
    return {NAN, NAN};
}

void GeometryHelper::getMostOuterPoints(const std::vector<Coordinate>& shape, Coordinate& leftMost,
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

bool GeometryHelper::isCrossingAntiMeridian(const std::vector<Coordinate>& shape) {
    Coordinate lastPoint = shape.back(); // first check is comparing last point of polygon with first point
    // check all points with the next one if they step over the antimeridian (aka from 179deg to -179 deg and vise versa)
    for (const auto& point : shape) {
        if (std::signbit(lastPoint.longitude) != std::signbit(point.longitude) && std::abs(lastPoint.longitude) > 100) {
            return true;
        }
        lastPoint = point;
    }
    return false;
}