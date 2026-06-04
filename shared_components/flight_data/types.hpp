#pragma once
#include <nlohmann/json.hpp>

struct Coordinate {
    float longitude;
    float latitude;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Coordinate, longitude, latitude);

    static bool isInvalid(Coordinate coord) {
        return coord.latitude < -91 || coord.latitude > 91 || coord.longitude < -181 || coord.longitude > 181;
    }

    std::string toString() const {
        return "Coordinate{latitude=" + std::to_string(latitude) + ", longitude=" + std::to_string(longitude) + "}";
    }
};

#define COORDINATE_INIT_INVALID() (Coordinate{-400, -400})

typedef std::vector<Coordinate> Route;

typedef Route PlannedRoute;
typedef Route FlightRoute;

enum class ConnectionState: uint8_t {
    CONNECTING = 0,
    CONNECTED = 1,
};

NLOHMANN_JSON_SERIALIZE_ENUM(ConnectionState, {
                             {ConnectionState::CONNECTING, "connecting"},
                             {ConnectionState::CONNECTED, "connected"},
                             })
