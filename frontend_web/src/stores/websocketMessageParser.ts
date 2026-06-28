import { ConnectionState, type ConnectionUpdatePacket } from '@/types/connection.ts'
import type { Coordinate } from '@/types/coordinates.ts'
import type { FlightUpdatePacket } from '@/types/flight.ts'

import type { StoreState } from '@/stores/store.ts'
import type { SensorUpdatePacket } from '@/types/sensor.ts'

/**
 * Normalizes Unix timestamps or ISO date strings into seconds since epoch.
 */
export function toTimeT(v: number | string | undefined | null): number | null {
  if (v == null) return null
  if (typeof v === 'number') {
    return v > 1e12 ? Math.floor(v / 1000) : Math.floor(v)
  }

  const n = Date.parse(v)
  return Number.isNaN(n) ? null : Math.floor(n / 1000)
}

/**
 * Converts common coordinate payload shapes into a normalized `Coordinate`.
 * Returns `null` for missing, invalid, or sentinel values.
 */
export function toCoordinate(v: unknown): Coordinate | null {
  if (v == null) return null

  let coord: Coordinate | null = null

  if (Array.isArray(v) && v.length >= 2) {
    coord = { latitude: Number(v[0]), longitude: Number(v[1]) }
  }

  if (typeof v === 'object') {
    const obj = v as Record<string, unknown>
    if ('lat' in obj && 'lon' in obj) {
      coord = { latitude: Number(obj.lat), longitude: Number(obj.lon) }
    }
    if ('latitude' in obj && 'longitude' in obj) {
      coord = { latitude: Number(obj.latitude), longitude: Number(obj.longitude) }
    }
  }

  if (coord?.longitude == 0 && coord?.latitude == 0) {
    coord = null
  }
  if (coord != null && (coord.latitude < -200 || coord.longitude < -200)) {
    coord = null
  }

  return coord
}

/**
 * Parses connection state values from either enum values or case-insensitive strings.
 */
export function parseConnectionState(v: unknown): ConnectionState | null {
  if (v == null) return null
  if (v === ConnectionState.CONNECTED || v === ConnectionState.CONNECTING) return v
  if (typeof v === 'string') {
    const s = v.toLowerCase()
    if (s === ConnectionState.CONNECTED) return ConnectionState.CONNECTED
    if (s === ConnectionState.CONNECTING) return ConnectionState.CONNECTING
  }

  return null
}

/**
 * Narrows a value to a plain record before accessing packet fields.
 */
export function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null
}

/**
 * Strips BOM/NUL characters and surrounding whitespace from websocket payloads.
 */
export function normalizeWebSocketPayload(raw: string): string {
  return raw.replace(/^\uFEFF/, '').trim().replace(/\u0000/g, '')
}

/**
 * Applies a flight packet to the shared store state.
 */
export function applyFlightPacket(store: StoreState, packet: FlightUpdatePacket): void {
  const basePos = toCoordinate(packet.basePosition)
  const planePos = toCoordinate(packet.planePosition)

  if (basePos) store.basePosition = basePos

  store.basePositionUpdateTime = toTimeT(packet.basePositionUpdateTime)

  if (planePos)
    store.planePosition = planePos

  store.planePositionUpdateTime = toTimeT(packet.planePositionUpdateTime)

  store.flightRoute = packet.flightRoute ?? null
  store.flightRouteUpdateTime = toTimeT(packet.flightRouteUpdateTime)

  store.plannedRoute = packet.plannedRoute ?? null
  store.plannedRouteUpdateTime = toTimeT(packet.plannedRouteUpdateTime)
}

/**
 * Applies a connection packet to the shared store state.
 */
export function applyConnectionPacket(store: StoreState, packet: ConnectionUpdatePacket): void {
  const baseState = parseConnectionState(packet.baseConnectionState)
  if (baseState) store.connectionStateBaseStation = baseState
  store.lastContactBaseStationTimestamp = toTimeT(packet.lastContactBaseStationTimestamp)

  const planeState = parseConnectionState(packet.planeConnectionState)
  if (planeState) store.connectionStatePlane = planeState
  store.lastContactPlaneTimestamp = toTimeT(packet.lastContactPlaneTimestamp)

  const gpsBase = parseConnectionState(packet.gpsConnectionBase)
  if (gpsBase) store.gpsConnectionBase = gpsBase
  const gpsPlane = parseConnectionState(packet.gpsConnectionPlane)
  if (gpsPlane) store.gpsConnectionPlane = gpsPlane

  const barometerBase = parseConnectionState(packet.barometerConnectionBase)
  if(barometerBase) store.barometerConnectionBase = barometerBase
  const barometerPlane = parseConnectionState(packet.barometerConnectionPlane)
  if(barometerPlane) store.barometerConnectionPlane = barometerPlane

  const motorCom = parseConnectionState(packet.motorComConnectionPlane)
  if (motorCom) store.motorComConnectionPlane = motorCom
  const magnetometer = parseConnectionState(packet.magnetometerConnectionPlane)
  if (magnetometer) store.magnetometerConnectionPlane = magnetometer
}


export function applySensorPacket(store: StoreState, packet: SensorUpdatePacket): void {
  const pressureBase = packet.barometerPressureBase
  if (pressureBase) store.pressureBase = pressureBase
  const pressurePlane = packet.barometerPressurePlane
  if (pressurePlane) store.pressurePlane = pressurePlane

  const calculatedAltitude = packet.calculatedAltitude
  if (calculatedAltitude) store.calculatedAltitude = calculatedAltitude
}
