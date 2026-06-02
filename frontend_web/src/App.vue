<script setup lang="ts">
import { RouterView } from 'vue-router'
import ConnectionStatusButton from '@/components/ConnectionStatusButton.vue'
import StepView from '@/components/StepView.vue'
</script>

<template>
  <div class="appShell">
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
