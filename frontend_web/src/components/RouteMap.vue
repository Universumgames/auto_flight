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
    planeHeading?: number | null
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
    planeHeading: null,
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

function createPlaneIcon(label: string, color: string, heading: number): L.DivIcon {
  return L.divIcon({
    className: 'position-marker-icon',
    html: `
      <div style="
        position: relative;
        width: 28px;
        height: 28px;
        transform: rotate(${heading}deg);
      ">
        <div style="
          position: absolute;
          top: -8px;
          left: 50%;
          transform: translateX(-50%);
          width: 0;
          height: 0;
          border-left: 5px solid transparent;
          border-right: 5px solid transparent;
          border-bottom: 8px solid ${color};
        "></div>
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
          transform: rotate(${-heading}deg);
        ">${label}</div>
      </div>
    `,
    iconSize: [28, 28],
    iconAnchor: [14, 14],
  })
}

function createPositionMarker(
  position: Coordinate,
  label: string,
  color: string,
  heading?: number | null,
): L.Marker {
  const icon = heading != null ? createPlaneIcon(label, color, heading) : createPositionIcon(label, color)
  const marker = L.marker([position.latitude, position.longitude], {
    icon,
    keyboard: false,
  })

  return marker
}

function destinationPoint(position: Coordinate, bearingDeg: number, distanceMeters: number): Coordinate {
  const R = 6371000
  const bearing = (bearingDeg * Math.PI) / 180
  const lat1 = (position.latitude * Math.PI) / 180
  const lon1 = (position.longitude * Math.PI) / 180
  const angularDistance = distanceMeters / R

  const lat2 = Math.asin(
    Math.sin(lat1) * Math.cos(angularDistance) + Math.cos(lat1) * Math.sin(angularDistance) * Math.cos(bearing),
  )
  const lon2 =
    lon1 +
    Math.atan2(
      Math.sin(bearing) * Math.sin(angularDistance) * Math.cos(lat1),
      Math.cos(angularDistance) - Math.sin(lat1) * Math.sin(lat2),
    )

  return {
    latitude: (lat2 * 180) / Math.PI,
    longitude: (lon2 * 180) / Math.PI,
  }
}

function createHeadingLine(position: Coordinate, heading: number, color: string): L.Polyline {
  const end = destinationPoint(position, heading, 50)

  return L.polyline(
    [
      [position.latitude, position.longitude],
      [end.latitude, end.longitude],
    ],
    {
      color,
      weight: 3,
      dashArray: '6 4',
      interactive: false,
    },
  )
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
    if (props.planeHeading != null) {
      positionMarkers.addLayer(createHeadingLine(props.planePosition, props.planeHeading, '#d97706'))
    }

    positionMarkers.addLayer(
      createPositionMarker(props.planePosition, 'PL', '#d97706', props.planeHeading),
    )
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
  [() => props.baseStationPosition, () => props.planePosition, () => props.planeHeading],
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
