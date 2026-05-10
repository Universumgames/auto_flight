import { reactive } from 'vue'
import { useWebSocket } from '@vueuse/core'

export const store = reactive({
  position: [51.316310347903176, 6.569530261539499],
})


const { status, data, send, open, close } = useWebSocket(`ws://${window.location.host}/api/ws`, {
  autoConnect: true,
  autoReconnect: true,
  onConnected(ws) {
    console.log('Connected!')
  },
  onDisconnected(ws, event) {
    console.log('Disconnected!', event.code)
  },
  onError(ws, event) {
    console.error('Error:', event)
  },
  onMessage(ws, event) {
    console.log('Message:', event.data)
  },
  heartbeat: {
    message: 'ping',
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-expect-error
    scheduler: (cb) => setInterval(cb, 2000),
    pongTimeout: 1000,
  },
})
