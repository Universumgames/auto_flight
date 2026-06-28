export enum ConnectionState {
  CONNECTING = 'connecting',
  CONNECTED = 'connected',
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
}

