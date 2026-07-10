export enum ConnectionState {
  CONNECTING = 'connecting',
  CONNECTED = 'connected',
}

export enum FlightState {
  PLANNING = 'planning',
  PLANNED = 'planned',
  FLYING = 'flying',
  RETURNING = 'returning',
}

export interface ConnectionUpdatePacket {
  type?: 'connection'
  baseConnectionState: ConnectionState
  lastContactBaseStationTimestamp: number
  planeConnectionState: ConnectionState
  lastContactPlaneTimestamp: number
  gpsConnectionBase?: ConnectionState
  gpsConnectionPlane?: ConnectionState
  barometerConnectionBase?: ConnectionState
  barometerConnectionPlane?: ConnectionState
  motorComConnectionPlane?: ConnectionState
  magnetometerConnectionPlane?: ConnectionState
  accelerometerConnectionPlane?: ConnectionState
  manualOverridePlane?: boolean
  flightState?: FlightState
}

