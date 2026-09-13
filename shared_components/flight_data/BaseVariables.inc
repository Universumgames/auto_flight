// X-macro list — base station state (there is exactly one base station).
// Include with FLIGHT_VAR_BASE(name, Name, type, initialValue, updateType, additionalCalls) defined.
FLIGHT_VAR_BASE(basePosition,                  BasePosition,                  Coordinate,       COORDINATE_INIT_INVALID(),   POSITION,   {})
FLIGHT_VAR_BASE(baseConnectionState,           BaseConnectionState,           ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_BASE(basePressure,                  BasePressure,                  float,            0.0f,                        SENSOR,     {})
FLIGHT_VAR_BASE(baseBarometerConnectionState,  BaseBarometerConnectionState,  ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_BASE(baseGPSConnectionState,        BaseGPSConnectionState,        ConnectionState,  ConnectionState::CONNECTING, CONNECTION, {})
FLIGHT_VAR_BASE(baseBatteryPercentage,         BaseBatteryPercentage,         int,              0,                           SENSOR,     {})
