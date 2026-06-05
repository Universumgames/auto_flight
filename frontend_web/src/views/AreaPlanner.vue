<script setup lang="ts">
import { onMounted, ref } from 'vue'
import PolygonMap from '@/components/PolygonMap.vue'
import { ConfigurationState, store } from '@/stores/store.ts'
import type { Coordinate } from '@/types/coordinates.ts'
import NextStepBtn from '@/components/NextStepBtn.vue'
import IconLoader from '@/components/icons/IconLoader.vue'
import { useRouter } from 'vue-router'

const polygon = ref<Coordinate[] | null>(null)
const loading = ref(false)

const router = useRouter()

const retrievePolygon = async (): Promise<Coordinate[] | null> => {
  const response = await fetch("/api/area", {
    method: "GET",
    headers: {
      "Content-Type": "application/json",
    },
  })
  if(!response.ok) {
    return null
  }
  const json = await response.json()
  return json.shape
}

onMounted(async () => {
  store.configurationState = ConfigurationState.AREA_SELECTION
  polygon.value = await retrievePolygon();
})

const submitPolygon = async () => {
  if (!polygon.value || loading.value) return
  loading.value = true
  try {
    const response = await fetch('/api/area', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ shape: polygon.value }),
    })

    if (!response.ok) {
      const text = await response.text().catch(() => '')
      throw new Error(
        `Failed to submit polygon - ${response.status} ${response.statusText} ${text}`,
      )
    }

    const data = await response.text().catch(() => null)
    const countResponse = parseInt(data ?? '', 10)
    if (countResponse == polygon.value.length) {
      console.log('Polygon submitted successfully:', data)
      // only change the configuration state on successful (2xx) response
      store.configurationState = ConfigurationState.ROUTE_APPROVAL
      await router.push({name: 'RoutePreview'})
    }else{
      console.error(`Unexpected response from server: expected ${polygon.value.length} but got ${data}`)
    }
  } catch (error) {
    console.error('Error submitting polygon:', error)
  } finally {
    loading.value = false
  }
}
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

    <NextStepBtn :disabled="!polygon || loading" @next="submitPolygon" />

    <!-- Loading overlay shown while waiting for API response -->
    <div v-if="loading" class="route-planner__overlay" aria-hidden="true">
      <div class="route-planner__spinner">
        <IconLoader />
      </div>
    </div>

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

.route-planner__button {
  padding: 0.5rem 1rem;
  border-radius: 12px;
  color: white;
  background-color: var(--color-primary-action);
}

/* overlay for loading */
.route-planner__overlay {
  position: fixed;
  inset: 0; /* top:0; right:0; bottom:0; left:0 */
  display: flex;
  align-items: center;
  justify-content: center;
  background: rgba(15, 23, 42, 0.45); /* semi-transparent dark */
  z-index: 9999;
}

.route-planner__spinner {
  background: rgba(255, 255, 255, 0.08);
  padding: 1rem;
  border-radius: 12px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
}
</style>
