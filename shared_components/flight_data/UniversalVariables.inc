// X-macro list of variables exposed via a universal, sourceId-keyed accessor.
// FlightStorageClass::BASE_ID proxies to the base implementation; any other id
// proxies to the plane implementation for that plane id.
//
// plannedRoute, flightRoute, plannedArea and flightState (see PlaneVariables.inc)
// don't need an entry here: they have no base counterpart, and their existing
// plane accessors already default gracefully for an id with no matching plane
// (BASE_ID included), so they already serve as their own universal accessor.

// Variables that exist on both base and plane.
// FLIGHT_VAR_UNIVERSAL(name, Name, type, BaseName, PlaneName)
FLIGHT_VAR_UNIVERSAL(position,                 Position,                 Coordinate,       BasePosition,                 PlanePosition)
FLIGHT_VAR_UNIVERSAL(connectionState,          ConnectionState,          ConnectionState,  BaseConnectionState,          PlaneConnectionState)
FLIGHT_VAR_UNIVERSAL(pressure,                 Pressure,                 float,            BasePressure,                 PlanePressure)
FLIGHT_VAR_UNIVERSAL(barometerConnectionState, BarometerConnectionState, ConnectionState,  BaseBarometerConnectionState, PlaneBarometerConnectionState)
FLIGHT_VAR_UNIVERSAL(gpsConnectionState,       GPSConnectionState,       ConnectionState,  BaseGPSConnectionState,       PlaneGPSConnectionState)
FLIGHT_VAR_UNIVERSAL(batteryPercentage,        BatteryPercentage,        int,              BaseBatteryPercentage,        PlaneBatteryPercentage)

// Variables that only exist on planes; sourceId == BASE_ID reports/discards `initialValue`.
// FLIGHT_VAR_UNIVERSAL_PLANE_ONLY(name, Name, type, initialValue, PlaneName)
FLIGHT_VAR_UNIVERSAL_PLANE_ONLY(heading,                      Heading,                      int,              0,                           PlaneHeading)
FLIGHT_VAR_UNIVERSAL_PLANE_ONLY(motorControlConnectionState,  MotorControlConnectionState,  ConnectionState,  ConnectionState::CONNECTING, PlaneMotorControlConnectionState)
FLIGHT_VAR_UNIVERSAL_PLANE_ONLY(magnetometerConnectionState,  MagnetometerConnectionState,  ConnectionState,  ConnectionState::CONNECTING, PlaneMagnetometerConnectionState)
FLIGHT_VAR_UNIVERSAL_PLANE_ONLY(accelerometerConnectionState, AccelerometerConnectionState, ConnectionState,  ConnectionState::CONNECTING, PlaneAccelerometerConnectionState)
FLIGHT_VAR_UNIVERSAL_PLANE_ONLY(manualOverride,               ManualOverride,               bool,             false,                       PlaneManualOverride)
