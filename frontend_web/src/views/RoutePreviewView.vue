<script setup lang="ts">
import { onMounted, ref } from 'vue'
import type { Coordinate } from '@/types/coordinates.ts'
import { ConfigurationState, store } from '@/stores/store.ts'

const fetchPlannedRoute = async (): Promise<Coordinate[]> => {
  const req = await fetch('/api/route', {})
  const data = await req.json()
  console.log(data)
  return data.route
}

const plannedRoute = ref<Coordinate[]>([])

onMounted(async () => {
  store.configurationState = ConfigurationState.ROUTE_APPROVAL
  plannedRoute.value = await fetchPlannedRoute()
})
</script>

<template>
  <h1>Route Preview</h1>
</template>

<style scoped></style>
