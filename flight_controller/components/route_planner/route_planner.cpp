#include "route_planner.hpp"

#include <cmath>

#include "geo_helper.hpp"
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


RoutePlannerClass::RoutePlannerClass() = default;

std::pair<Coordinate, Coordinate> RoutePlannerClass::getShapeIntersection(
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

std::vector<std::pair<Coordinate, Coordinate>> RoutePlannerClass::generateSimpleSweepLines(
    const std::vector<Coordinate>& shape, const float swathDistance) {
    Coordinate leftMost{}, rightMost{}, topMost{}, bottomMost{};
    getMostOuterPoints(shape, leftMost, rightMost, topMost, bottomMost);
    Coordinate topLeft = {leftMost.longitude, topMost.latitude};
    Coordinate bottomRight = {rightMost.longitude, bottomMost.latitude};

    const float latDiff = topMost.latitude - bottomMost.latitude - swathDistance * 0.9f;
    const int lineCount = std::ceil(latDiff / swathDistance);
    const float lineDistance = latDiff / (float)lineCount;

    std::vector<std::pair<Coordinate, Coordinate>> lines;
    for (int i = 0; i < lineCount; i++) {
        std::pair<Coordinate, Coordinate> line = {
            {topLeft.longitude, topLeft.latitude - (float)i * lineDistance - 0.5f * swathDistance},
            {bottomRight.longitude, topLeft.latitude - (float)i * lineDistance - 0.5f * swathDistance}
        };
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::pair<Coordinate, Coordinate>> RoutePlannerClass::generateContainedSimpleSweepLines(const std::vector<Coordinate>& shape, float swathDistance) {
    auto simpleLines = generateSimpleSweepLines(shape, swathDistance);
    std::vector<std::pair<Coordinate, Coordinate>> lines;
    for (const auto& line : simpleLines) {
        auto intersections = getShapeIntersection(shape, line.first, line.second);
        if (!std::isnan(intersections.first.latitude) && !std::isnan(intersections.first.longitude) &&
            !std::isnan(intersections.second.latitude) && !std::isnan(intersections.second.longitude)) {
            lines.push_back(intersections);
        }
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

bool RoutePlannerClass::isCrossingAntiMeridian(const std::vector<Coordinate>& shape) {
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


std::vector<Coordinate> RoutePlannerClass::generateSimpleSweepPath(const std::vector<Coordinate>& shape, float swathDistance) {
    const auto lines = generateContainedSimpleSweepLines(shape, swathDistance);
    std::vector<Coordinate> path;
    Coordinate lastPoint = lines[0].first;
    bool startLeft = true;
    for (const auto& line : lines) {
        auto nextPoint = startLeft? line.first : line.second;
        auto nextNextPoint = startLeft? line.second : line.first;
        const auto linesDist = lastPoint.latitude - nextPoint.latitude;
        Coordinate intermediate = Coordinate{nextPoint.longitude + linesDist * (startLeft ? -1 : 1), nextPoint.latitude + 0.7f * linesDist};
        Coordinate intermediate2 = {nextPoint.longitude + (intermediate.longitude - nextPoint.longitude) * 0.8f, nextPoint.latitude + 0.1f * linesDist};

        path.push_back(intermediate);
        path.push_back(intermediate2);
        path.push_back(nextPoint);
        path.push_back(nextNextPoint);
        startLeft = !startLeft;
        lastPoint = nextNextPoint;
    }
    return path;
}


std::vector<Coordinate> RoutePlannerClass::planRoute(const std::vector<Coordinate>& shape, float maxPointDistance, float maxSwathWidth, float overlapFactor) {
    auto rawPath = generateSimpleSweepPath(shape, maxSwathWidth * (1.0f - overlapFactor));
    std::vector<Coordinate> path;

    Coordinate lastPoint = rawPath.front();
    for (const auto& point : rawPath) {
        auto distMeters = distanceInMeters(lastPoint, point);
        if (distMeters < maxPointDistance) {
            path.push_back(point);
            lastPoint = point;
            continue;
        }

        auto interpolationPointCount = std::ceil(distMeters / maxPointDistance);
        for (int i = 1; i < interpolationPointCount; i++) {
            float t = (float)i / (float)interpolationPointCount;
            Coordinate interpolatedPoint = {
                lastPoint.latitude + t * (point.latitude - lastPoint.latitude),
                lastPoint.longitude + t * (point.longitude - lastPoint.longitude)
            };
            path.push_back(interpolatedPoint);
        }
        path.push_back(point);
        lastPoint = point;
    }

    return path;
}
