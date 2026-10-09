#pragma once
#include "../flight_com/types.hpp"

float latitudeDiffToMeters(float latitudeDiff);
float longitudeDiffToMeters(float longitudeDiff, float atLatitude);

float distanceInMeters(Coordinate coordA, Coordinate coordB);

float metersToLatitudeDegree(float meters);