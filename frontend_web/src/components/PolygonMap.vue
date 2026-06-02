<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import L from 'leaflet'
import 'leaflet/dist/leaflet.css'
import 'leaflet-draw'
import 'leaflet-draw/dist/leaflet.draw.css'

import type { Coordinate } from '@/types/coordinates.ts'
import IconLocator from '@/components/icons/IconLocator.vue'

type DrawEvents = {
  CREATED: 'draw:created'
  EDITED: 'draw:edited'
  DELETED: 'draw:deleted'
}

type LeafletDrawOptions = {
  draw: {
    polyline: false
    rectangle: false
    circle: false
    circlemarker: false
    marker: false
    polygon: {
      allowIntersection: false
      showArea: true
      shapeOptions: {
        color: string
        fillColor: string
        fillOpacity: number
      }
    }
  }
  edit: {
    featureGroup: L.FeatureGroup
    remove: boolean
  }
}

type LeafletDrawStatic = typeof L & {
  Control: {
    Draw: new (options: LeafletDrawOptions) => L.Control
  }
  Draw?: {
    Event: DrawEvents
  }
}

const props = withDefaults(
  defineProps<{
    modelValue?: Coordinate[] | null
    baseStationPosition?: Coordinate | null
    planePosition?: Coordinate | null
    center?: Coordinate
    zoom?: number
    height?: string
  }>(),
  {
    center: () => ({ latitude: 51.316310347903176, longitude: 6.569530261539499 }),
    zoom: 15,
    height: '600px',
    modelValue: null,
    baseStationPosition: null,
    planePosition: null,
  },
)

const emit = defineEmits<{
  (event: 'update:modelValue', value: Coordinate[] | null): void
  (event: 'created', value: Coordinate[]): void
  (event: 'ready', value: L.Map): void
}>()

const mapContainer = ref<HTMLDivElement | null>(null)
const height = computed(() => props.height)

let map: L.Map | null = null
let drawnItems: L.FeatureGroup | null = null
let positionMarkers: L.LayerGroup | null = null
let activePolygon: L.Polygon | null = null

function fitToHome() {
  if (!map) return
  map.setView([props.baseStationPosition?.latitude ?? props.center.latitude, props.baseStationPosition?.longitude ?? props.center.longitude], props.zoom)
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

function polygonToCoordinates(layer: L.Polygon): Coordinate[] {
  const latLngs = layer.getLatLngs()
  const ring = Array.isArray(latLngs[0]) ? (latLngs[0] as L.LatLng[]) : (latLngs as L.LatLng[])

  return ring.map((point) => ({ latitude: point.lat, longitude: point.lng }))
}

function createPolygon(coords: Coordinate[]): L.Polygon {
  return L.polygon(
    coords.map((coord) => [coord.latitude, coord.longitude] as [number, number]),
    {
      color: '#2563eb',
      fillColor: '#60a5fa',
      fillOpacity: 0.2,
    },
  )
}

function setPolygonFromCoordinates(coords: Coordinate[] | null) {
  if (!map || !drawnItems) return

  drawnItems.clearLayers()
  activePolygon = null

  if (!coords?.length) return

  const polygon = createPolygon(coords)
  drawnItems.addLayer(polygon)
  activePolygon = polygon

  const bounds = polygon.getBounds()
  if (bounds.isValid()) {
    map.fitBounds(bounds.pad(0.15))
  }
}

function syncPositionMarkers() {
  if (!map || !positionMarkers) return

  positionMarkers.clearLayers()

  if (props.baseStationPosition) {
    positionMarkers.addLayer(createPositionMarker(props.baseStationPosition, 'BS', '#2563eb'))
  }

  if (props.planePosition) {
    positionMarkers.addLayer(createPositionMarker(props.planePosition, 'PL', '#d97706'))
  }
}

function emitPolygonState(coords: Coordinate[] | null) {
  emit('update:modelValue', coords)
  if (coords) {
    emit('created', coords)
  }
}

onMounted(() => {
  if (!mapContainer.value) return

  // eslint-disable-next-line @typescript-eslint/ban-ts-comment
  //@ts-expect-error
  ;(window as unknown).type = true

  const leafletMap = L.map(mapContainer.value)
  map = leafletMap

  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; OpenStreetMap contributors',
  }).addTo(leafletMap)

  positionMarkers = L.layerGroup().addTo(leafletMap)
  drawnItems = L.featureGroup().addTo(leafletMap)

  const leafletDraw = L as LeafletDrawStatic

  const drawControl = new leafletDraw.Control.Draw({
    draw: {
      polyline: false,
      rectangle: false,
      circle: false,
      circlemarker: false,
      marker: false,
      polygon: {
        allowIntersection: false,
        showArea: true,
        shapeOptions: {
          color: '#2563eb',
          fillColor: '#60a5fa',
          fillOpacity: 0.2,
        },
      },
    },
    edit: {
      featureGroup: drawnItems,
      remove: true,
    },
  })

  leafletMap.addControl(drawControl)

  const drawEvents: DrawEvents = leafletDraw.Draw?.Event ?? {
    CREATED: 'draw:created',
    EDITED: 'draw:edited',
    DELETED: 'draw:deleted',
  }

  leafletMap.on(drawEvents.CREATED, (event: { layerType?: string; layer?: L.Layer }) => {
    if (event.layerType !== 'polygon' || !event.layer) return

    drawnItems?.clearLayers()
    drawnItems?.addLayer(event.layer)

    activePolygon = event.layer as L.Polygon
    emitPolygonState(polygonToCoordinates(activePolygon))
  })

  leafletMap.on(drawEvents.EDITED, () => {
    if (!activePolygon) return

    emit('update:modelValue', polygonToCoordinates(activePolygon))
  })

  leafletMap.on(drawEvents.DELETED, () => {
    activePolygon = null
    emit('update:modelValue', null)
  })

  if (props.modelValue?.length) {
    setPolygonFromCoordinates(props.modelValue)
    emit('update:modelValue', props.modelValue)
  } else {
    fitToHome()
  }

  syncPositionMarkers()

  emit('ready', leafletMap)
})

watch(
  () => props.modelValue,
  (coords) => {
    setPolygonFromCoordinates(coords ?? null)
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
  drawnItems = null
  positionMarkers = null
  activePolygon = null
})
</script>

<template>
  <div class="polygon-map-shell">
    <button @click="fitToHome" class="return-home-btn" title="Return to home view">
      <IconLocator/>
    </button>
    <div ref="mapContainer" class="polygon-map"></div>
  </div>
</template>

<style scoped>
.polygon-map-shell {
  width: 100%;
  position: relative;
}

.polygon-map {
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
