#pragma once
#include <vector>

struct Coordinate {
    float longitude;
    float latitude;
};


struct Vector2D {
    float x, y;

public:
    Coordinate asCoordinate() const {
        return Coordinate{.longitude = x, .latitude = y};
    }

    Vector2D operator-(const Vector2D& other) const {
        return Vector2D{x - other.x, y - other.y};
    }
    Vector2D operator+(const Vector2D& other) const {
        return Vector2D{x + other.x, y + other.y};
    }
};



typedef std::vector<Coordinate> Route;