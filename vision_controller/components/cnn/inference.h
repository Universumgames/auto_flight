#pragma once

struct Prediction {
    int         index;       // class index
    const char* label;       // class name
    float       confidence;  // 0..1
};