#pragma once
#include <cmath>
#include <cstddef>

// Fixed-size running average over the last N pushed samples.
template <size_t N>
class MovingAverage {
    static_assert(N > 0, "MovingAverage window size must be greater than 0");

public:
    float push(float value) {
        sum -= buffer[index];
        buffer[index] = value;
        sum += value;
        index = (index + 1) % N;
        if (count < N) count++;
        return sum / static_cast<float>(count);
    }

private:
    float buffer[N] = {};
    float sum = 0.0f;
    size_t index = 0;
    size_t count = 0;
};

// Running average over the last N heading samples (degrees, [0, 360)).
// Averages via sin/cos components so it wraps correctly around the 0/360 boundary
// instead of e.g. averaging 359 and 1 into 180.
template <size_t N>
class HeadingMovingAverage {
public:
    float push(float headingDeg) {
        const float rad = headingDeg * static_cast<float>(M_PI) / 180.0f;
        const float sinAvg = sinAverage.push(sinf(rad));
        const float cosAvg = cosAverage.push(cosf(rad));
        float avgDeg = atan2f(sinAvg, cosAvg) * 180.0f / static_cast<float>(M_PI);
        if (avgDeg < 0.0f) avgDeg += 360.0f;
        return avgDeg;
    }

private:
    MovingAverage<N> sinAverage;
    MovingAverage<N> cosAverage;
};
