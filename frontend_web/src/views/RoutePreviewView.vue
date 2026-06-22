<script setup lang="ts">
import { onMounted, ref } from 'vue'
import type { Coordinate } from '@/types/coordinates.ts'
import { ConfigurationState, store } from '@/stores/store.ts'
import PolygonMap from '@/components/PolygonMap.vue'
import RouteMap from '@/components/RouteMap.vue'

const fetchPlannedRoute = async (): Promise<Coordinate[]> => {
  const req = await fetch('/api/route', {})
  const data = await req.json()
  console.log(data)
  return data.route
}

const plannedRoute = ref<Coordinate[]>([])
const flightHistory = ref<Coordinate[]>([])
const waitingPlannedRoute = ref<boolean>(false)

onMounted(async () => {
  store.configurationState = ConfigurationState.ROUTE_APPROVAL
  waitingPlannedRoute.value = true
  let route: Coordinate[] = []
  let tries = 0
  while (route.length == 0 || tries > 50) {
    route = await fetchPlannedRoute()
    tries++
    await new Promise(resolve => setTimeout(resolve, 1000))
  }
  plannedRoute.value = route
})
</script>

<template>
  <h1>Route Preview</h1>
  <RouteMap :planned-route="plannedRoute" :flight-history="flightHistory" :base-station-position="store.basePosition" :plane-position="store.planePosition"/>
</template>

<style scoped></style>
