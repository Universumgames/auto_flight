// X-macro list — per-plane state, stored keyed by plane device id (see FlightStorageClass::planes).
// Include with FLIGHT_VAR_PLANE(name, Name, type, initialValue, updateType, additionalCalls) defined.
FLIGHT_VAR_PLANE(planePosition,                     PlanePosition,                     Coordinate,       COORDINATE_INIT_INVALID(),   POSITION,   {
    if (distanceInMeters(originalValue, value) > 15) {
        addPointToFlightRoute(value, planeId);
    }
})
FLIGHT_VAR_PLANE(planeConnectionState,              PlaneConnectionState,              ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_PLANE(planePressure,                     PlanePressure,                     float,            0.0f,                        SENSOR,     {})
FLIGHT_VAR_PLANE(planeHeading,                      PlaneHeading,                      int,              0,                           SENSOR,     {})
FLIGHT_VAR_PLANE(planeBarometerConnectionState,     PlaneBarometerConnectionState,     ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_PLANE(planeMotorControlConnectionState,  PlaneMotorControlConnectionState,  ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_PLANE(planeGPSConnectionState,           PlaneGPSConnectionState,           ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_PLANE(planeMagnetometerConnectionState,  PlaneMagnetometerConnectionState,  ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_PLANE(planeAccelerometerConnectionState, PlaneAccelerometerConnectionState, ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_PLANE(planeManualOverride,               PlaneManualOverride,               bool,             false,                       CONNECTION, {})
FLIGHT_VAR_PLANE(planeBatteryPercentage,            PlaneBatteryPercentage,            int,              0,                           BATTERY,     {})
FLIGHT_VAR_PLANE(plannedRoute,                      PlannedRoute,                      PlannedRoute,     PlannedRoute{},              ROUTE,      {})
FLIGHT_VAR_PLANE(flightRoute,                       FlightRoute,                       FlightRoute,      FlightRoute{},               HISTORY,    {})
FLIGHT_VAR_PLANE(plannedArea,                       PlannedArea,                       std::vector<Coordinate>, std::vector<Coordinate>{}, AREA, { info.plannedRoute.clear(); })
FLIGHT_VAR_PLANE(flightState,                       FlightState,                       FlightState,      FlightState::PLANNING,       STATE,      {})
