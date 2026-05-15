<script setup lang="ts">
import { ref } from 'vue'
import PolygonMap from '@/components/PolygonMap.vue'
import { store } from '@/stores/store.ts'
import type { Coordinate } from '@/types/coordinates.ts'

const polygon = ref<Coordinate[] | null>(null)
</script>

<template>
  <div class="route-planner">
    <header class="route-planner__header">
      <h1>Route Planner</h1>
      <p>Draw a polygon on the map to define the area for your route.</p>
    </header>

    <PolygonMap
      v-model="polygon"
      :base-station-position="store.basePosition"
      :plane-position="store.planePosition"
    />

    <section class="route-planner__details">
      <h2>Current polygon</h2>
      <pre v-if="polygon">{{ JSON.stringify(polygon, null, 2) }}</pre>
      <p v-else>No polygon drawn yet.</p>
    </section>
  </div>
</template>

<style scoped>
.route-planner {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  padding: 1.5rem;
}

.route-planner__header h1,
.route-planner__details h2 {
  margin: 0;
}

.route-planner__header p {
  margin: 0.25rem 0 0;
  color: #4b5563;
}

.route-planner__details {
  padding: 1rem;
  border-radius: 12px;
}

.route-planner__details pre {
  margin: 0.75rem 0 0;
  overflow: auto;
  font-size: 0.875rem;
}
</style>
