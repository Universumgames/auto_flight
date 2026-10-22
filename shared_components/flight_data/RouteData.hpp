#pragma once
#include <vector>

#include "geo_helper.hpp"
#include "types.hpp"

// RouteSettings is generated from flight_com/packets.jtd.json (via types.hpp)

struct AreaData {
    std::vector<Coordinate> areaPoints;
    RouteSettings settings;

    AreaData()
        : areaPoints(), settings() {}

    AreaData(const std::vector<Coordinate>& points, RouteAlgorithm algorithm, int overlap) : areaPoints(points),
        settings(RouteSettings(algorithm, overlap)) {}

    AreaData(const std::vector<Coordinate>& points, const RouteSettings& settings) : areaPoints(points),
        settings(settings) {}

    [[nodiscard]] bool operator==(const AreaData& other) const {
        return areaPoints == other.areaPoints &&
            settings == other.settings;
    }

    [[nodiscard]] bool isInitialized() const {
        return !areaPoints.empty();
    }

    [[nodiscard]] const std::vector<Coordinate>& getAreaPoints() const {
        return areaPoints;
    }

    [[nodiscard]] RouteAlgorithm getRouteAlgorithm() const {
        return settings.routeAlgorithm;
    }

    [[nodiscard]] int getOverlapPercentage() const {
        return settings.overlapPercentage;
    }

    [[nodiscard]] const RouteSettings& getSettings() const {
        return settings;
    }
};

struct RouteData {
private:
    std::vector<Coordinate> routePoints;
    std::vector<Coordinate> summarizedRoutePoints; // optional, can be empty
    RouteSettings settings;
    size_t hash;
    float length;

public:
    RouteData()
        : routePoints(), summarizedRoutePoints(), settings(), length(0) {}

    RouteData(const std::vector<Coordinate>& points, const std::vector<Coordinate>& summarizedPoints, RouteAlgorithm algorithm, int overlap) : routePoints(points),
        summarizedRoutePoints(summarizedPoints), settings(RouteSettings(algorithm, overlap)) {
        calculateHash();
        calculateLength();
    }

    RouteData(const std::vector<Coordinate>& points, const std::vector<Coordinate>& summarizedPoints, const RouteSettings& settings) : routePoints(points),
        summarizedRoutePoints(summarizedPoints), settings(settings) {
        calculateHash();
        calculateLength();
    }

#if FLIGHT_DEVICE_TYPE_BASE_STATION
    RouteData(const std::vector<Coordinate>& points, const RouteSettings& settings, size_t hash) : routePoints(points),
        settings(settings), hash(hash) {}
#endif

    [[nodiscard]] bool operator==(const RouteData& other) const {
        return routePoints == other.routePoints &&
            summarizedRoutePoints == other.summarizedRoutePoints &&
            settings == other.settings;
    }

    [[nodiscard]] bool isInitialized() const {
        return !routePoints.empty();
    }

    [[nodiscard]] const std::vector<Coordinate>& getRoutePoints() const {
        return routePoints;
    }

    [[nodiscard]] const std::vector<Coordinate>& getSummarizedRoutePoints() const {
        return summarizedRoutePoints;
    }

    [[nodiscard]] size_t getHash() const {
        return hash;
    }

    [[nodiscard]] RouteAlgorithm getRouteAlgorithm() const {
        return settings.routeAlgorithm;
    }

    [[nodiscard]] int getOverlapPercentage() const {
        return settings.overlapPercentage;
    }

    [[nodiscard]] const RouteSettings& getSettings() const {
        return settings;
    }

    /** Get the length of the route in meters. Returns -1 if the length is not calculated. */
    [[nodiscard]] float getRouteLength() const {
        return length;
    }

    void clear() {
        routePoints.clear();
        summarizedRoutePoints.clear();
        settings = RouteSettings();
        hash = 0;
    }

private:
    void calculateHash() {
        // Implement a hash calculation for the routePoints vector
        // This is a placeholder implementation; you can replace it with a proper hash function
        hash = settings.hash();
        for (const auto& point : routePoints) {
            hash ^= std::hash<double>()(point.latitude) ^ std::hash<double>()(point.longitude);
        }
    }

    void calculateLength() {
        // Implement a length calculation for the routePoints vector
        // This is a placeholder implementation; you can replace it with a proper length calculation
        length = 0;
        for (size_t i = 1; i < routePoints.size(); ++i) {
            length += distanceInMeters(routePoints[i - 1], routePoints[i]);
        }
    }
};
