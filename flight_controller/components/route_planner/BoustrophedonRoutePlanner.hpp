#pragma once
#include "IRoutePlanner.hpp"

class BoustrophedonRoutePlanner : public IRoutePlanner {
public:
    ~BoustrophedonRoutePlanner() override = default;
    std::vector<Coordinate> planRoute(const std::vector<Coordinate>& shape, float overlapFactor) override;
    [[nodiscard]] RouteAlgorithm getImplementedAlgorithm() const override {
        return RouteAlgorithm::BOUSTROPHEDON;
    }

    bool supportsAlgorithm(RouteAlgorithm algorithm) const override {
        return algorithm == RouteAlgorithm::BOUSTROPHEDON || algorithm == RouteAlgorithm::BASIC;
    }

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
};