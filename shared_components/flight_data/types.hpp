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

    bool operator==(const Coordinate& b) const {
        return longitude == b.longitude && latitude == b.latitude;
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

enum class FlightState: uint8_t {
    PLANNING = 0x22,
    PLANNED = 0x33,
    FLYING = 0x44,
    RETURNING = 0x55
};

NLOHMANN_JSON_SERIALIZE_ENUM(FlightState, {
                             {FlightState::PLANNING, "planning"},
                             {FlightState::PLANNED, "planned"},
                             {FlightState::FLYING, "flying"},
                             {FlightState::RETURNING, "returning"},
                             })
