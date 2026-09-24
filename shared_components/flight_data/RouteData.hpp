#pragma once
#include <vector>
#include "types.hpp"

struct RouteSettings {
    RouteAlgorithm routeAlgorithm;
    int overlapPercentage;

    bool operator==(const RouteSettings& other) const {
        return routeAlgorithm == other.routeAlgorithm && overlapPercentage == other.overlapPercentage;
    }

    size_t hash() const {
        size_t h1 = std::hash<int>()(static_cast<int>(routeAlgorithm));
        size_t h2 = std::hash<int>()(overlapPercentage);
        return h1 ^ (h2 << 1); // Combine the two hashes
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RouteSettings, routeAlgorithm, overlapPercentage)
};

struct AreaData {
    std::vector<Coordinate> areaPoints;
    RouteSettings settings;

    AreaData()
        : areaPoints(), settings() {}

    AreaData(const std::vector<Coordinate>& points, RouteAlgorithm algorithm, int overlap) : areaPoints(points),
        settings(RouteSettings(algorithm, overlap)) {
    }

    AreaData(const std::vector<Coordinate>& points, const RouteSettings& settings) : areaPoints(points), settings(settings) {
    }

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
    RouteSettings settings;
    size_t hash;

public:
    RouteData()
        : routePoints(), settings() {}

    RouteData(const std::vector<Coordinate>& points, RouteAlgorithm algorithm, int overlap) : routePoints(points),
        settings(RouteSettings(algorithm, overlap)) {
        calculateHash();
    }

    RouteData(const std::vector<Coordinate>& points, const RouteSettings& settings) : routePoints(points), settings(settings) {
        calculateHash();
    }

    [[nodiscard]] bool operator==(const RouteData& other) const {
        return routePoints == other.routePoints &&
               settings == other.settings;
    }

    [[nodiscard]] bool isInitialized() const {
        return !routePoints.empty();
    }

    [[nodiscard]] const std::vector<Coordinate>& getRoutePoints() const {
        return routePoints;
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

    void clear() {
        routePoints.clear();
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
};
