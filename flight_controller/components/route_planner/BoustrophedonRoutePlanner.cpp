#include "BoustrophedonRoutePlanner.hpp"

#include "esp_log.h"
#include "geometry_helper.hpp"

std::vector<Coordinate> BoustrophedonRoutePlanner::planRoute(const std::vector<Coordinate>& shape, float overlapFactor) {
    auto rawPath = generateSimpleSweepPath(shape, 40 * (1.0f - overlapFactor));
    if (rawPath.empty()) {
        std::cerr << "No raw path generated for the given shape, cannot plan route" << std::endl;
        return {};
    }
    ESP_LOGI("RoutePlanner", "Generated raw path with %d points", rawPath.size());
    return rawPath;
}


std::vector<std::pair<Coordinate, Coordinate>> BoustrophedonRoutePlanner::generateSimpleSweepLines(
    const std::vector<Coordinate>& shape, const float swathDistance) {
    if (shape.size() < 3) {
        std::cerr << "Shape must have at least 3 points to generate sweep lines" << std::endl;
        return {};
    }
    Coordinate leftMost{}, rightMost{}, topMost{}, bottomMost{};
    GeometryHelper::getMostOuterPoints(shape, leftMost, rightMost, topMost, bottomMost);
    Coordinate topLeft = {.longitude = leftMost.longitude, .latitude = topMost.latitude};
    Coordinate bottomRight = {.longitude = rightMost.longitude, .latitude = bottomMost.latitude};

    ESP_LOGI("RoutePlanner", "Shape bounds: leftMost=(%.6f, %.6f), rightMost=(%.6f, %.6f), topMost=(%.6f, %.6f), bottomMost=(%.6f, %.6f)",
             leftMost.longitude, leftMost.latitude, rightMost.longitude, rightMost.latitude,
             topMost.longitude, topMost.latitude, bottomMost.longitude, bottomMost.latitude);


    const double latDiff = topMost.latitude - bottomMost.latitude - swathDistance * 0.9f;
    const int lineCount = std::ceil(latDiff / swathDistance);
    const double lineDistance = latDiff / (float)lineCount;

    ESP_LOGI("RoutePlanner", "Generating %d sweep lines with distance %.6f degrees for shape with lat diff %.6f degrees, with swathDistance %.6f",
             lineCount, lineDistance, latDiff, swathDistance);

    std::vector<std::pair<Coordinate, Coordinate>> lines;
    for (int i = 0; i < lineCount; i++) {
        std::pair<Coordinate, Coordinate> line = {
            {topLeft.longitude, topLeft.latitude - (float)(i * lineDistance) - 0.5f * swathDistance},
            {bottomRight.longitude, topLeft.latitude - (float)(i * lineDistance) - 0.5f * swathDistance}
        };
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::pair<Coordinate, Coordinate>> BoustrophedonRoutePlanner::generateContainedSimpleSweepLines(const std::vector<Coordinate>& shape, float swathDistance) {
    auto simpleLines = generateSimpleSweepLines(shape, swathDistance);
    if (simpleLines.empty()) {
        std::cerr << "No simple sweep lines generated for the given shape, cannot generate contained lines" << std::endl;
        return {};
    }
    std::vector<std::pair<Coordinate, Coordinate>> lines;
    for (const auto& line : simpleLines) {
        auto intersections = GeometryHelper::getShapeIntersection(shape, line.first, line.second);
        if (!std::isnan(intersections.first.latitude) && !std::isnan(intersections.first.longitude) &&
            !std::isnan(intersections.second.latitude) && !std::isnan(intersections.second.longitude)) {
            lines.push_back(intersections);
        }
    }
    return lines;
}


std::vector<Coordinate> BoustrophedonRoutePlanner::generateSimpleSweepPath(const std::vector<Coordinate>& shape, float swathDistance) {
    const auto lines = generateContainedSimpleSweepLines(shape, swathDistance);
    if (lines.empty()) {
        std::cerr << "No sweep lines generated for the given shape, cannot generate path" << std::endl;
        return {};
    }
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