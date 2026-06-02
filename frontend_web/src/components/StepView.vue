<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useMediaQuery } from '@vueuse/core'
import { useRoute } from 'vue-router'
import { ConfigurationState } from '@/stores/store.ts'
import StepEntry from '@/components/StepEntry.vue'

const route = useRoute()
const isCompactLayout = useMediaQuery('(max-width: 56rem)')
const isExpanded = ref(true)

watch(
  isCompactLayout,
  (compact) => {
    isExpanded.value = !compact
  },
  { immediate: true },
)

const panelOpen = computed(() => !isCompactLayout.value || isExpanded.value)
</script>

<template>
  <div class="stepContainer" :class="{ 'stepContainer--compact': isCompactLayout }">
    <button
      v-if="isCompactLayout"
      type="button"
      class="stepToggle"
      @click="isExpanded = !isExpanded"
    >
      Flight Planner {{ isExpanded ? '▾' : '▸' }}
    </button>

    <div v-show="panelOpen" class="stepContent">
      <h1 v-if="!isCompactLayout">Flight Planner</h1>
      <div class="stepsList">
        <StepEntry name="Establish Connection" :state="ConfigurationState.CONNECTION" />
        <StepEntry name="Select Area" :state="ConfigurationState.AREA_SELECTION" />
        <StepEntry name="Review Route" :state="ConfigurationState.ROUTE_APPROVAL" />
        <StepEntry name="Prepare for flight" :state="ConfigurationState.STARTING"/>
        <StepEntry name="Observe flight" :state="ConfigurationState.FLYING"/>
        <StepEntry name="Finishing" :state="ConfigurationState.FINISHING"/>
      </div>
    </div>
  </div>
</template>

<style scoped>
.stepContainer {
  background-color: var(--color-background-soft);
  padding: 1rem;
  border-radius: 1rem;
  min-width: 30ch;
  max-width: 50vw;
  box-sizing: border-box;
  overflow: auto;
}

.stepContainer--compact {
  width: 100%;
  max-width: 100%;
  min-width: 0;
  flex: 0 0 auto;
}

.stepToggle {
  width: 100%;
  margin-bottom: 0.75rem;
  padding: 0.5rem 0.75rem;
  border: 0;
  border-radius: 0.75rem;
  background: var(--color-background-mute);
  color: inherit;
  font: inherit;
  text-align: left;
}

.stepContent {
  min-width: 0;
}

.stepsList {
  margin-top: 1rem;
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 1rem;
}
</style>
