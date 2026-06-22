<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import L from 'leaflet'
import 'leaflet/dist/leaflet.css'

import type { Coordinate } from '@/types/coordinates.ts'
import IconLocator from '@/components/icons/IconLocator.vue'

const props = withDefaults(
  defineProps<{
    plannedRoute?: Coordinate[]
    flightHistory?: Coordinate[]
    baseStationPosition?: Coordinate | null
    planePosition?: Coordinate | null
    center?: Coordinate
    zoom?: number
    height?: string
  }>(),
  {
    center: () => ({
      latitude: 51.316310347903176,
      longitude: 6.569530261539499,
    }),
    zoom: 15,
    height: '600px',
    plannedRoute: () => [],
    flightHistory: () => [],
    baseStationPosition: null,
    planePosition: null,
  },
)

const emit = defineEmits<{
  (event: 'ready', value: L.Map): void
}>()

const mapContainer = ref<HTMLDivElement | null>(null)
const height = computed(() => props.height)

let map: L.Map | null = null
let routeLayers: L.LayerGroup | null = null
let positionMarkers: L.LayerGroup | null = null

let hasAutoFitted = false

function fitToHome() {
  if (!map) return

  map.setView(
    [
      props.baseStationPosition?.latitude ?? props.center.latitude,
      props.baseStationPosition?.longitude ?? props.center.longitude,
    ],
    props.zoom,
  )
}

function fitToRoutes() {
  if (!map) return

  const bounds = L.latLngBounds([])

  props.plannedRoute.forEach((c) => {
    bounds.extend([c.latitude, c.longitude])
  })

  props.flightHistory.forEach((c) => {
    bounds.extend([c.latitude, c.longitude])
  })

  if (bounds.isValid()) {
    map.fitBounds(bounds.pad(0.1))
  } else {
    fitToHome()
  }
}

function createPositionIcon(label: string, color: string): L.DivIcon {
  return L.divIcon({
    className: 'position-marker-icon',
    html: `
      <div style="
        width: 28px;
        height: 28px;
        border-radius: 9999px;
        background: ${color};
        color: white;
        border: 2px solid white;
        box-shadow: 0 2px 8px rgba(15, 23, 42, 0.35);
        display: flex;
        align-items: center;
        justify-content: center;
        font-size: 11px;
        font-weight: 700;
        line-height: 1;
      ">${label}</div>
    `,
    iconSize: [28, 28],
    iconAnchor: [14, 14],
  })
}

function createPositionMarker(position: Coordinate, label: string, color: string): L.Marker {
  return L.marker([position.latitude, position.longitude], {
    icon: createPositionIcon(label, color),
    keyboard: false,
  })
}

function createPolyline(coords: Coordinate[], color: string, weight = 4): L.Polyline {
  return L.polyline(
    coords.map((coord) => [coord.latitude, coord.longitude] as [number, number]),
    {
      color,
      weight,
    },
  )
}

function updateRoutes() {
  if (!routeLayers) return

  routeLayers.clearLayers()

  const bounds = L.latLngBounds([])
  let hasData = false

  if (props.plannedRoute.length > 1) {
    routeLayers.addLayer(createPolyline(props.plannedRoute, '#2563eb'))

    props.plannedRoute.forEach((c) => {
      bounds.extend([c.latitude, c.longitude])
    })

    hasData = true
  }

  if (props.flightHistory.length > 1) {
    routeLayers.addLayer(createPolyline(props.flightHistory, '#dc2626'))

    props.flightHistory.forEach((c) => {
      bounds.extend([c.latitude, c.longitude])
    })

    hasData = true
  }

  if (!hasAutoFitted && hasData && bounds.isValid() && map) {
    map.fitBounds(bounds.pad(0.1))
    hasAutoFitted = true
  }
}

function syncPositionMarkers() {
  if (!positionMarkers) return

  positionMarkers.clearLayers()

  if (props.baseStationPosition) {
    positionMarkers.addLayer(createPositionMarker(props.baseStationPosition, 'BS', '#2563eb'))
  }

  if (props.planePosition) {
    positionMarkers.addLayer(createPositionMarker(props.planePosition, 'PL', '#d97706'))
  }

  if (props.plannedRoute.length > 0) {
    positionMarkers.addLayer(createPositionMarker(props.plannedRoute[0]!, 'S', '#16a34a'))

    positionMarkers.addLayer(
      createPositionMarker(props.plannedRoute[props.plannedRoute.length - 1]!, 'E', '#dc2626'),
    )
  }
}

onMounted(() => {
  if (!mapContainer.value) return

  const leafletMap = L.map(mapContainer.value)
  map = leafletMap

  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; OpenStreetMap contributors',
  }).addTo(leafletMap)

  routeLayers = L.layerGroup().addTo(leafletMap)
  positionMarkers = L.layerGroup().addTo(leafletMap)

  updateRoutes()
  syncPositionMarkers()

  if (!hasAutoFitted) {
    fitToHome()
  }

  emit('ready', leafletMap)
})

watch(
  () => props.plannedRoute,
  () => {
    hasAutoFitted = false
    updateRoutes()
    syncPositionMarkers()
  },
  { deep: true },
)

watch(
  () => props.flightHistory,
  () => {
    updateRoutes()
    syncPositionMarkers()
  },
  { deep: true },
)

watch(
  [() => props.baseStationPosition, () => props.planePosition],
  () => {
    syncPositionMarkers()
  },
  { deep: true },
)

onBeforeUnmount(() => {
  map?.remove()

  map = null
  routeLayers = null
  positionMarkers = null
})
</script>

<template>
  <div class="route-map-shell">
    <button class="return-home-btn" title="Show complete route" @click="fitToRoutes">
      <IconLocator />
    </button>

    <div ref="mapContainer" class="route-map" />
  </div>
</template>

<style scoped>
.route-map-shell {
  width: 100%;
  position: relative;
}

.route-map {
  width: 100%;
  height: v-bind(height);
  border-radius: 12px;
  overflow: hidden;
}

.return-home-btn {
  position: absolute;
  top: 12px;
  right: 12px;
  z-index: 1000;
  width: 40px;
  height: 40px;
  border: none;
  border-radius: 8px;
  background: white;
  box-shadow: 0 2px 8px rgba(15, 23, 42, 0.15);
  cursor: pointer;
  font-size: 20px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s ease;
}

.return-home-btn:hover {
  background: #f1f5f9;
  box-shadow: 0 4px 12px rgba(15, 23, 42, 0.25);
  transform: scale(1.05);
}

.return-home-btn:active {
  transform: scale(0.95);
}
</style>
