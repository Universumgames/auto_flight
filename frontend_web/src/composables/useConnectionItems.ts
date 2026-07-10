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
            ? ` (${store.basePosition!.latitude.toFixed(6)}, ${store.basePosition!.longitude.toFixed(6)})`
            : ''),
        state: isConnected(store.gpsConnectionBase) ? 'done' : 'loading',
      },
      {
        label:
          'Barometer' + (store.pressureBase != 0 ? ` (${store.pressureBase.toFixed(2)}hPa)` : ''),
        state: isConnected(store.barometerConnectionBase) ? 'done' : 'loading',
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
            ? ` (${store.planePosition!.latitude.toFixed(6)}, ${store.planePosition!.longitude.toFixed(6)})`
            : ''),
        state: isConnected(store.gpsConnectionPlane) ? 'done' : 'loading',
      },
      {
        label: 'Motor Controller',
        state: isConnected(store.motorComConnectionPlane) ? 'done' : 'loading',
      },
      {
        label: 'Autopilot Control' + (store.manualOverridePlane ? ' (manual override active)' : ''),
        state: store.manualOverridePlane ? 'loading' : 'done',
      },
      {
        label: 'Magnetometer',
        state: isConnected(store.magnetometerConnectionPlane) ? 'done' : 'loading',
      },
      {
        label: 'Accelerometer',
        state: isConnected(store.accelerometerConnectionPlane) ? 'done' : 'loading',
      },
      {
        label:
          'Barometer' + (store.pressurePlane != 0 ? ` (${store.pressurePlane.toFixed(2)}hPa)` : ''),
        state: isConnected(store.barometerConnectionPlane) ? 'done' : 'loading',
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

// true when the plane and its motor controller are connected but autopilot control is disabled (manual override active)
export const autopilotDisabledWarning = computed(
  () =>
    isConnected(store.connectionStatePlane) &&
    isConnected(store.motorComConnectionPlane) &&
    store.manualOverridePlane,
)

// true when the plane is connected but its motor controller is not
export const motorControllerDisconnectedError = computed(
  () => isConnected(store.connectionStatePlane) && !isConnected(store.motorComConnectionPlane),
)
