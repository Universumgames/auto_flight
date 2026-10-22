#pragma once
#include <vector>

// Coordinate, ConnectionState, FlightState, RouteAlgorithm, RouteSettings, PacketType and the
// protocol constants are generated from packets.jtd.json, see gen_packets.py.
#include "FlightTypes.generated.hpp"

#define COORDINATE_INIT_INVALID() (Coordinate{COORDINATE_INVALID_VALUE, COORDINATE_INVALID_VALUE})

typedef std::vector<Coordinate> Route;

typedef Route PlannedRoute;
typedef Route FlightRoute;
