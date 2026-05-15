import { useWebSocket } from '@vueuse/core'
import { reactive } from 'vue'

import { ConnectionState } from '@/types/connection.ts'
import { createWebSocketStoreState, handleWebSocketMessage } from '@/stores/websocketMessage.ts'

export const store = reactive(createWebSocketStoreState())

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
  },
  onError(ws, event) {
    console.error('Error:', event)
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

