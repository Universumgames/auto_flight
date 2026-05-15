import { ConnectionState, type ConnectionUpdatePacket } from '@/types/connection.ts'
import type { Coordinate } from '@/types/coordinates.ts'
import type { FlightRoute, FlightUpdatePacket, PlannedRoute } from '@/types/flight.ts'

import {
  applyConnectionPacket,
  applyFlightPacket,
  isRecord,
  normalizeWebSocketPayload,
} from '@/stores/websocketMessageParser.ts'

export function createWebSocketStoreState() {
  return {
    position: { latitude: 51.316310347903176, longitude: 6.569530261539499 } as Coordinate,
    connectionStatePlane: ConnectionState.CONNECTING,
    connectionStateBaseStation: ConnectionState.CONNECTING,
    basePosition: null as Coordinate | null,
    basePositionUpdateTime: null as number | null,
    planePosition: null as Coordinate | null,
    planePositionUpdateTime: null as number | null,
    flightRoute: null as FlightRoute | null,
    flightRouteUpdateTime: null as number | null,
    plannedRoute: null as PlannedRoute | null,
    plannedRouteUpdateTime: null as number | null,
    lastContactBaseStationTimestamp: null as number | null,
    lastContactPlaneTimestamp: null as number | null,
    gpsConnectionBase: ConnectionState.CONNECTING,
    gpsConnectionPlane: ConnectionState.CONNECTING,
  }
}

export type WebSocketStoreState = ReturnType<typeof createWebSocketStoreState>


export function handleWebSocketMessage(store: WebSocketStoreState, event: MessageEvent): void {
  try {
    const raw = typeof event.data === 'string' ? normalizeWebSocketPayload(event.data) : null
    if (!raw) return

    if (raw == 'pong') {
      store.connectionStateBaseStation = ConnectionState.CONNECTED
      return
    }

    const parsed = JSON.parse(raw) as unknown
    if (!isRecord(parsed)) return

    const typeField = typeof parsed.type === 'string' ? parsed.type : undefined
    if (!typeField) return

    if (typeField === 'flight') {
      applyFlightPacket(store, parsed as unknown as FlightUpdatePacket)
    } else if (typeField === 'connection') {
      applyConnectionPacket(store, parsed as unknown as ConnectionUpdatePacket)
    } else {
      console.debug('Unknown packet type received', parsed.type, parsed)
    }
  } catch (err) {
    console.debug('websocket message ignored or failed to parse as JSON:', err)
  }
}
