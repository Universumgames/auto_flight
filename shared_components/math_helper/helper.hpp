#pragma once

inline float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

inline float clamp01(float value) {
    return clamp(value, 0.0f, 1.0f);
}