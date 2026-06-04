import { ConnectionState, type ConnectionUpdatePacket } from '@/types/connection.ts'
import type { Coordinate } from '@/types/coordinates.ts'
import type { FlightRoute, FlightUpdatePacket, PlannedRoute } from '@/types/flight.ts'

import {
  applyConnectionPacket,
  applyFlightPacket,
  applySensorPacket,
  isRecord,
  normalizeWebSocketPayload,
} from '@/stores/websocketMessageParser.ts'
import type { StoreState } from '@/stores/store.ts'
import type { SensorUpdatePacket } from '@/types/sensor.ts'

export function handleWebSocketMessage(store: StoreState, event: MessageEvent): void {
  try {
    const raw = typeof event.data === 'string' ? normalizeWebSocketPayload(event.data) : null
    if (!raw) return

    if (raw == 'pong') {
      store.connectionStateBaseStation = ConnectionState.CONNECTED
      return
    }

    const parsed = JSON.parse(raw) as unknown
    if (!isRecord(parsed)) return

    console.log(parsed)

    const typeField = typeof parsed.type === 'string' ? parsed.type : undefined
    if (!typeField) return

    if (typeField === 'flight') {
      applyFlightPacket(store, parsed as unknown as FlightUpdatePacket)
    } else if (typeField === 'connection') {
      applyConnectionPacket(store, parsed as unknown as ConnectionUpdatePacket)
    } else if (typeField === 'sensor') {
      applySensorPacket(store, parsed as unknown as SensorUpdatePacket)
    } else {
      console.debug('Unknown packet type received', parsed.type, parsed)
    }
  } catch (err) {
    console.debug('websocket message ignored or failed to parse as JSON:', err)
  }
}
