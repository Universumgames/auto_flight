<script setup lang="ts">
import { computed } from 'vue'
import { RouterView, useRoute } from 'vue-router'
import ConnectionStatusButton from '@/components/ConnectionStatusButton.vue'
import StepView from '@/components/StepView.vue'
import {
  autopilotDisabledWarning,
  gpsPlaneUnavailableError,
  motorControllerDisconnectedError,
} from '@/composables/useConnectionItems.ts'

const route = useRoute()
const isConnectionPage = computed(() => route.name === 'Connection')
</script>

<template>
  <div class="appShell">
    <div v-if="motorControllerDisconnectedError" class="warningBanner warningBanner--error">
      <span class="warningBanner__icon" aria-hidden="true">✕</span>
      <span>Motor controller disconnected</span>
    </div>
    <div
      v-else-if="gpsPlaneUnavailableError && !isConnectionPage"
      class="warningBanner warningBanner--error"
    >
      <span class="warningBanner__icon" aria-hidden="true">✕</span>
      <span>No GPS position available for the plane</span>
    </div>
    <div v-else-if="autopilotDisabledWarning" class="warningBanner warningBanner--warning">
      <span class="warningBanner__icon" aria-hidden="true">⚠</span>
    <span>Autopilot control is disabled — manual override active</span>
    </div>
    <header style="position:relative; padding: 0.75rem 1rem;">
      <div style="position:absolute; right:1rem; top:0.5rem;">
        <ConnectionStatusButton />
      </div>
    </header>

    <main class="appMain">
      <div class="routePane">
        <RouterView />
      </div>
      <StepView />
    </main>
  </div>
</template>

<style scoped>
@import "leaflet/dist/leaflet.css";

.appShell {
  display: flex;
  flex-direction: column;
}

.warningBanner {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  padding: 0.55rem 0.9rem;
  border-radius: 0.6rem;
  border: 1px solid;
  background: var(--color-background-soft);
  font-weight: 600;
  font-size: 0.9rem;
}

.warningBanner__icon {
  flex-shrink: 0;
  width: 1.5rem;
  height: 1.5rem;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 999px;
  font-size: 0.85rem;
  line-height: 1;
  color: white;
}

.warningBanner--error {
  border-color: rgba(220, 38, 38, 0.45);
  color: #dc2626;
}

.warningBanner--error .warningBanner__icon {
  background: #dc2626;
}

.warningBanner--warning {
  border-color: rgba(245, 158, 11, 0.5);
  color: #b45309;
}

.warningBanner--warning .warningBanner__icon {
  background: linear-gradient(135deg, #f97316 0%, #f59e0b 100%);
}

main,
.appMain {
  display: flex;
  gap: 1rem;
  align-items: stretch;
  flex: 1 1 auto;
  min-height: 0;
}

.routePane {
  flex: 1 1 auto;
  min-width: 0;
  min-height: 0;
}

@media (max-width: 56rem) {
  .appMain {
    flex-direction: column-reverse;
  }

  .routePane {
    width: 100%;
  }
}
</style>
