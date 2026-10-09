#include "IRoutePlanner.hpp"

#include "esp_log.h"

std::vector<Coordinate> IRoutePlanner::calculateIntermediatePoints(const std::vector<Coordinate>& rawPath, float maxPointDistance) {
    std::vector<Coordinate> path;

    Coordinate lastPoint = rawPath.front();
    for (const auto& point : rawPath) {
        auto distMeters = distanceInMeters(lastPoint, point);
        if (distMeters < maxPointDistance) {
            path.push_back(point);
            lastPoint = point;
            continue;
        }

        auto interpolationPointCount = static_cast<int>(std::ceil(distMeters / maxPointDistance));
        ESP_LOGI("RoutePlanner", "Interpolating %.2f meters between (%.6f, %.6f) and (%.6f, %.6f) with %d points",
                 distMeters, lastPoint.latitude, lastPoint.longitude, point.latitude, point.longitude, interpolationPointCount);
        for (int i = 1; i < interpolationPointCount; i++) {
            float t = (float)i / (float)interpolationPointCount;
            Coordinate interpolatedPoint = {
                .longitude = lastPoint.longitude + t * (point.longitude - lastPoint.longitude),
                .latitude = lastPoint.latitude + t * (point.latitude - lastPoint.latitude)
            };
            path.push_back(interpolatedPoint);
        }
        path.push_back(point);
        lastPoint = point;
    }

    ESP_LOGI("RoutePlanner", "Generated final path with %d points after interpolation", path.size());

    return path;
}
