import type { Coordinate, Route } from '@/types/coordinates.ts'

export type PlannedRoute = Route
export type FlightRoute = Route

export interface FlightUpdatePacket {
  type?: 'flight'
  basePosition: Coordinate
  basePositionUpdateTime: number
  planePosition: Coordinate
  planePositionUpdateTime: number
  flightRoute: FlightRoute
  flightRouteUpdateTime: number
  plannedRoute: PlannedRoute
  plannedRouteUpdateTime: number
}

