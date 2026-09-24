#pragma once
#include "homog2d.hpp"
#include "types.hpp"
#include "geo_helper.hpp"

using namespace h2d;

class IRoutePlanner {
public:
    virtual ~IRoutePlanner() = default;

    /**
     * Plan a route for the given shape using the specified algorithm.
     * @param shape The shape to cover
     * @param overlapFactor The percentage of overlap between two paths (0.0 - 0.9)
     * @return The planned route
     */
    virtual std::vector<Coordinate> planRoute(const std::vector<Coordinate>& shape, float overlapFactor) = 0;

    [[nodiscard]] virtual RouteAlgorithm getImplementedAlgorithm() const = 0;

    [[nodiscard]] virtual bool supportsAlgorithm(RouteAlgorithm algorithm) const {
        return algorithm == getImplementedAlgorithm();
    }

    static std::vector<Coordinate> calculateIntermediatePoints(const std::vector<Coordinate>& rawPath, float maxPointDistance);


};
