#pragma once
#include <nlohmann/json.hpp>

struct Coordinate {
    float longitude;
    float latitude;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Coordinate, longitude, latitude);
};

#define COORDINATE_INIT_INVALID() (Coordinate{-400, -400})

typedef std::vector<Coordinate> Route;

typedef Route PlannedRoute;
typedef Route FlightRoute;

enum class ConnectionState {
    CONNECTING,
    CONNECTED,
};

NLOHMANN_JSON_SERIALIZE_ENUM(ConnectionState, {
                             {ConnectionState::CONNECTING, "connecting"},
                             {ConnectionState::CONNECTED, "connected"},
                             })
