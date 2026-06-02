import { useWebSocket } from '@vueuse/core'
import { reactive } from 'vue'

import { ConnectionState, type ConnectionUpdatePacket } from '@/types/connection.ts'
import { handleWebSocketMessage } from '@/stores/websocketMessage.ts'
import type { FlightRoute, FlightUpdatePacket, PlannedRoute } from '@/types/flight.ts'
import type { Coordinate } from '@/types/coordinates.ts'

export enum ConfigurationState {
  CONNECTION,
  AREA_SELECTION,
  ROUTE_APPROVAL,
  STARTING,
  FLYING,
  FINISHING,
}

export function createStoreState() {
  return {
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
    configurationState: ConfigurationState.CONNECTION,
    barometerConnectionBase: ConnectionState.CONNECTING,
    barometerConnectionPlane: ConnectionState.CONNECTING,
    pressureBase: 0.0,
    pressurePlane: 0.0,
    calculatedAltitude: 0.0
  }
}

export type StoreState = ReturnType<typeof createStoreState>


export const store = reactive(createStoreState())

useWebSocket(`ws://${window.location.host}/api/ws`, {
  autoConnect: true,
  autoReconnect: true,
  onConnected(_ws) {
    console.log('Connected!')
    store.connectionStateBaseStation = ConnectionState.CONNECTED
  },
  onDisconnected(ws, event) {
    console.log('Disconnected!', event.code)
    store.connectionStateBaseStation = ConnectionState.CONNECTING
    store.connectionStatePlane = ConnectionState.CONNECTING
  },
  onError(ws, event) {
    console.warn('Error:', event)
  },
  onMessage(ws, event) {
    handleWebSocketMessage(store, event)
  },
  heartbeat: {
    message: 'ping',
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-expect-error
    scheduler: (cb) => setInterval(cb, 2000),
    pongTimeout: 5000,
  },
})
