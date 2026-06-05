<script setup lang="ts">
import { connectionItems, isConnected, formatStatus } from '@/composables/useConnectionItems'
import { computed, onMounted } from 'vue'
import { ConfigurationState, store } from '@/stores/store.ts'
import NextStepBtn from '@/components/NextStepBtn.vue'

onMounted(() => {
  store.configurationState = ConfigurationState.CONNECTION
})

const allConnected = computed(() => {
  return connectionItems.value.every((item) => isConnected(item.status))
})
</script>

<template>
  <section class="connection-overview">
    <header class="connection-overview__header">
      <h2>Connection Overview</h2>
      <p>Live connection status and the sub-tasks that make each link ready.</p>
    </header>

    <ul class="connection-list">
      <li v-for="item in connectionItems" :key="item.label" class="connection-card">
        <div class="connection-card__row">
          <span
            class="status-icon"
            :class="isConnected(item.status) ? 'status-icon--connected' : 'status-icon--loading'"
            aria-hidden="true"
          >
            <span v-if="isConnected(item.status)">✓</span>
            <svg
              v-else
              class="status-spinner"
              viewBox="0 0 24 24"
              role="presentation"
              aria-hidden="true"
            >
              <g>
                <circle class="status-spinner__track" cx="12" cy="12" r="9"></circle>
                <circle class="status-spinner__dash" cx="12" cy="12" r="9"></circle>
                <animateTransform
                  attributeName="transform"
                  type="rotate"
                  from="0 12 12"
                  to="360 12 12"
                  dur="0.9s"
                  repeatCount="indefinite"
                />
              </g>
            </svg>
          </span>

          <div class="connection-card__text">
            <div class="connection-card__title-row">
              <h3>{{ item.label }}</h3>
              <span class="connection-card__badge">{{ formatStatus(item.status) }}</span>
            </div>

            <ul class="subtask-list">
              <li v-for="subTask in item.subTasks" :key="subTask.label" class="subtask-list__item">
                <svg
                  v-if="subTask.state === 'loading'"
                  class="subtask-spinner"
                  viewBox="0 0 24 24"
                  role="presentation"
                  aria-hidden="true"
                >
                  <g>
                    <circle class="subtask-spinner__track" cx="12" cy="12" r="9"></circle>
                    <circle class="subtask-spinner__dash" cx="12" cy="12" r="9"></circle>
                    <animateTransform
                      attributeName="transform"
                      type="rotate"
                      from="0 12 12"
                      to="360 12 12"
                      dur="0.9s"
                      repeatCount="indefinite"
                    />
                  </g>
                </svg>
                <span class="subtask-list__label">
                  {{ subTask.label }}
                </span>
              </li>
            </ul>
          </div>
        </div>
      </li>
      <NextStepBtn @next="$router.push({ name: 'AreaPlanner' })" :disabled="!allConnected" />
    </ul>
  </section>
</template>

<style scoped>
.connection-overview {
  padding: 1.25rem;
  border: 1px solid var(--color-border);
  border-radius: 1rem;
  background: var(--color-background-soft);
  box-shadow: 0 12px 30px rgba(15, 23, 42, 0.08);
  color: var(--color-text);
}

.connection-overview__header {
  margin-bottom: 1rem;
}

.connection-overview__header h2 {
  margin: 0;
  font-size: 1.1rem;
  font-weight: 700;
  color: var(--color-heading);
}

.connection-overview__header p {
  margin: 0.35rem 0 0;
  color: var(--color-text);
  font-size: 0.92rem;
}

.connection-list {
  list-style: none;
  padding: 0;
  margin: 0;
  display: grid;
  gap: 0.85rem;
}

.connection-card {
  border-radius: 0.9rem;
  background: var(--color-background);
  border: 1px solid var(--color-border);
  padding: 1rem;
}

.connection-card__row {
  display: flex;
  align-items: flex-start;
  gap: 0.9rem;
}

.status-icon {
  width: 1.8rem;
  height: 1.8rem;
  border-radius: 999px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 auto;
  margin-top: 0.1rem;
  font-size: 1rem;
  line-height: 1;
  color: white;
}

.status-icon--connected {
  background: #22c55e;
  box-shadow: 0 0 0 0.2rem rgba(34, 197, 94, 0.14);
}

.status-icon--loading {
  color: var(--color-border-hover);
}

.connection-card__text {
  flex: 1;
  min-width: 0;
}

.connection-card__title-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  margin-bottom: 0.65rem;
}

.connection-card__title-row h3 {
  margin: 0;
  font-size: 1rem;
  font-weight: 700;
}

.connection-card__badge {
  display: inline-flex;
  align-items: center;
  padding: 0.25rem 0.6rem;
  border-radius: 999px;
  font-size: 0.78rem;
  font-weight: 600;
  color: var(--color-text);
  background: var(--color-background-mute);
}

.subtask-list {
  list-style: none;
  padding: 0;
  margin: 0;
  display: grid;
  gap: 0.45rem;
}

.subtask-list__item {
  display: flex;
  align-items: center;
  gap: 0.55rem;
  color: var(--color-text);
  font-size: 0.92rem;
}

.subtask-spinner {
  width: 0.9rem;
  height: 0.9rem;
  display: inline-block;
  flex: 0 0 auto;
  color: var(--color-border-hover);
}

.status-spinner,
.subtask-spinner {
  display: block;
}

.status-spinner {
  width: 1.8rem;
  height: 1.8rem;
  display: block;
}

.status-spinner__track,
.subtask-spinner__track {
  fill: none;
  stroke: var(--color-border);
  stroke-width: 2;
}

.status-spinner__dash,
.subtask-spinner__dash {
  fill: none;
  stroke: currentColor;
  stroke-linecap: round;
  stroke-width: 2;
  stroke-dasharray: 18 40;
  stroke-dashoffset: 0;
}

@media (max-width: 640px) {
  .connection-overview {
    padding: 1rem;
  }

  .connection-card__title-row {
    flex-direction: column;
    align-items: flex-start;
    gap: 0.35rem;
  }
}
</style>
