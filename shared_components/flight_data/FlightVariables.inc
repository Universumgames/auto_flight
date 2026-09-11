// X-macro list — include with FLIGHT_VAR(name, Name, type, initialValue, updateType, additionalCalls) defined
FLIGHT_VAR(plannedRoute,                      PlannedRoute,                      PlannedRoute,              PlannedRoute{},                ROUTE,      {})
FLIGHT_VAR(flightRoute,                       FlightRoute,                       FlightRoute,               FlightRoute{},                 HISTORY,    {})
FLIGHT_VAR(basePosition,                      BasePosition,                      Coordinate,                COORDINATE_INIT_INVALID(),     POSITION,   {})
FLIGHT_VAR(planePosition,                     PlanePosition,                     Coordinate,                COORDINATE_INIT_INVALID(),     POSITION,   {
    if (distanceInMeters(originalValue, value) > 15) {
        addPointToFlightRoute(value);
    }
})
FLIGHT_VAR(baseConnectionState,               BaseConnectionState,               ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(planeConnectionState,              PlaneConnectionState,              ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(basePressure,                      BasePressure,                      float,                     0.0f,                          SENSOR,     {})
FLIGHT_VAR(planePressure,                     PlanePressure,                     float,                     0.0f,                          SENSOR,     {})
FLIGHT_VAR(planeHeading,                      PlaneHeading,                      int,                       0,                             SENSOR,     {})
FLIGHT_VAR(plannedArea,                       PlannedArea,                       std::vector<Coordinate>,   std::vector<Coordinate>{},     AREA,       { plannedRoute.clear(); })
FLIGHT_VAR(baseBarometerConnectionState,      BaseBarometerConnectionState,      ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(planeBarometerConnectionState,     PlaneBarometerConnectionState,     ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(planeMotorControlConnectionState,  PlaneMotorControlConnectionState,  ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(baseGPSConnectionState,            BaseGPSConnectionState,            ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(planeGPSConnectionState,           PlaneGPSConnectionState,           ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(planeMagnetometerConnectionState,  PlaneMagnetometerConnectionState,  ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(planeAccelerometerConnectionState, PlaneAccelerometerConnectionState, ConnectionState,           ConnectionState::CONNECTING,   CONNECTION, {})
FLIGHT_VAR(planeManualOverride,               PlaneManualOverride,               bool,                      false,                         CONNECTION, {})
FLIGHT_VAR(flightState,                       FlightState,                       FlightState,               FlightState::PLANNING,         STATE,      {})
FLIGHT_VAR(planeBatteryPercentage,            PlaneBatteryPercentage,            int,                       0,                             SENSOR,     {})
FLIGHT_VAR(baseBatteryPercentage,             BaseBatteryPercentage,             int,                       0,                             SENSOR,     {})
