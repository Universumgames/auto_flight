#include "geo_helper.hpp"

#include <cmath>

float latitudeDiffToMeters(float latitudeDiff) {
    return distanceInMeters({0, 0}, {.longitude = 0, .latitude = latitudeDiff});
}

float longitudeDiffToMeters(float longitudeDiff, float atLatitude) {
    return distanceInMeters({.longitude = 0, .latitude = atLatitude}, {.longitude = longitudeDiff, .latitude = atLatitude});
}

// Source - https://stackoverflow.com/a/11172685
// Posted by b-h-, modified by community. See post 'Timeline' for change history
// Retrieved 2026-04-28, License - CC BY-SA 4.0
float distanceInMeters(const Coordinate coordA, const Coordinate coordB) {
    double R = 6378.137; // Radius of earth in KM
    double dLat = coordB.latitude * M_PI / 180.0 - coordA.latitude * M_PI / 180.0;
    double dLon = coordB.longitude * M_PI / 180.0 - coordA.longitude * M_PI / 180.0;
    double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
        cos(coordA.latitude * M_PI / 180.0) * cos(coordB.latitude * M_PI / 180.0) *
        sin(dLon / 2.0) * sin(dLon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1 - a));
    double d = R * c;
    return d * 1000.0; // meters
}

float metersToLatitudeDegree(float meters) {
    // 1 degree of latitude is approximately 111.32 km
    return meters / 111320.0f;
}
