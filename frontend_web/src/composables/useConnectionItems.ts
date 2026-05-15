import { computed } from 'vue'
import { ConnectionState } from '@/types/connection.ts'
import { store } from '@/stores/store.ts'

export type SubTaskState = 'done' | 'loading'

export type SubTask = {
  label: string
  state: SubTaskState
}

export type ConnectionItem = {
  label: string
  status: ConnectionState
  subTasks: SubTask[]
}

export const isConnected = (status: ConnectionState) => status === ConnectionState.CONNECTED

export const connectionItems = computed<ConnectionItem[]>(() => [
  {
    label: 'Base Station',
    status: store.connectionStateBaseStation,
    subTasks: [
      {
        label: 'Connection',
        state: isConnected(store.connectionStateBaseStation) ? 'done' : 'loading',
      },
      {
        label:
          'GPS Position' +
          (store.basePosition != null
            ? ` (${store.basePosition!.latitude}, ${store.basePosition!.longitude})`
            : ''),
        state: isConnected(store.gpsConnectionBase) ? 'done' : 'loading',
      },
      {
        label: 'Barometer',
        state: 'loading',
      },
    ],
  },
  {
    label: 'Plane',
    status: store.connectionStatePlane,
    subTasks: [
      {
        label: 'Connection',
        state: isConnected(store.connectionStatePlane) ? 'done' : 'loading',
      },
      {
        label:
          'GPS Position' +
          (store.planePosition != null
            ? ` (${store.planePosition!.latitude}, ${store.planePosition!.longitude})`
            : ''),
        state: isConnected(store.gpsConnectionPlane) ? 'done' : 'loading',
      },
      {
        label: 'Motor Controller',
        state: 'loading',
      },
      {
        label: 'Accelerometer',
        state: 'loading',
      },
      {
        label: 'Barometer',
        state: 'loading',
      },
    ],
  },
])

export const formatStatus = (status: ConnectionState) =>
  status.charAt(0).toUpperCase() + status.slice(1)

// total connected flag — true only when all items are CONNECTED
export const totalIsConnected = computed(() =>
  connectionItems.value.every((i) => isConnected(i.status)),
)
