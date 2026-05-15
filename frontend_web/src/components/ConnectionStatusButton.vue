<script setup lang="ts">
import { ref, onMounted, onBeforeUnmount } from 'vue'
import { connectionItems, isConnected, formatStatus, totalIsConnected } from '@/composables/useConnectionItems'

const open = ref(false)
const root = ref<HTMLElement | null>(null)

const toggle = () => {
  open.value = !open.value
}

function onDocClick(e: MouseEvent) {
  const target = e.target as Node
  if (!root.value) return
  if (!root.value.contains(target)) {
    open.value = false
  }
}

onMounted(() => document.addEventListener('click', onDocClick))
onBeforeUnmount(() => document.removeEventListener('click', onDocClick))
</script>

<template>
  <div class="conn-status-root" ref="root">
    <button
      class="conn-status-button"
      @click.prevent.stop="toggle"
      :aria-expanded="open"
      aria-haspopup="true"
      title="Connection status"
    >
      <span
        class="status-icon"
        :class="totalIsConnected ? 'status-icon--connected' : 'status-icon--loading'"
        aria-hidden="true"
      >
        <span v-if="totalIsConnected">✓</span>
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
    </button>

    <div v-show="open" class="conn-popup" role="dialog" aria-label="Connection overview">
      <ul class="connection-list-compact">
        <li v-for="item in connectionItems" :key="item.label" class="connection-card-compact">
          <div class="connection-card-compact__row">
            <span
              class="mini-status"
              :class="isConnected(item.status) ? 'mini-status--connected' : 'mini-status--loading'"
              aria-hidden="true"
            >
              <span v-if="isConnected(item.status)">✓</span>
              <svg v-else class="mini-spinner" viewBox="0 0 24 24" role="presentation" aria-hidden="true">
                <g>
                  <circle class="mini-spinner__track" cx="12" cy="12" r="9"></circle>
                  <circle class="mini-spinner__dash" cx="12" cy="12" r="9"></circle>
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

            <div class="connection-card-compact__text">
              <div class="connection-card-compact__title-row">
                <strong>{{ item.label }}</strong>
                <span class="connection-card-compact__badge">{{ formatStatus(item.status) }}</span>
              </div>

              <ul class="subtask-list-compact">
                <li v-for="subTask in item.subTasks" :key="subTask.label" class="subtask-list-compact__item">
                  <span class="subtask-dot" :class="subTask.state === 'done' ? 'subtask-dot--done' : 'subtask-dot--loading'">
                    <span v-if="subTask.state === 'done'">✓</span>
                    <span v-else>⟳</span>
                  </span>
                  <span class="subtask-list-compact__label">{{ subTask.label }}</span>
                </li>
              </ul>
            </div>
          </div>
        </li>
      </ul>
    </div>
  </div>
</template>

<style scoped>
.conn-status-root {
  position: relative;
  display: inline-block;
}

.conn-status-button {
  border: none;
  background: rgba(255,255,255,0.02);
  padding: 0.12rem;
  cursor: pointer;
  border-radius: 999px;
  display: inline-flex;
  align-items: center;
  transition: transform 120ms ease, box-shadow 120ms ease;
}

.conn-status-button:focus {
  outline: none;
  transform: translateY(-1px);
}

.status-icon {
  width: 2.4rem;
  height: 2.4rem;
  border-radius: 999px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  font-size: 1.05rem;
  line-height: 1;
  color: white;
  transition: transform 180ms ease, box-shadow 180ms ease, background-color 180ms ease;
}

.status-icon--connected {
  background: #16a34a; /* stronger green */
  box-shadow: 0 6px 18px rgba(16, 185, 129, 0.18), 0 0 0 6px rgba(16,185,129,0.06);
}

.status-icon--loading {
  background: linear-gradient(135deg,#f97316 0%, #f59e0b 100%);
  color: white;
  box-shadow: 0 6px 20px rgba(249,115,22,0.18), 0 0 0 6px rgba(249,115,22,0.06);
  animation: conn-pulse 1.6s cubic-bezier(.4,0,.2,1) infinite;
}

.status-spinner { width: 1.6rem; height: 1.6rem; }
.status-spinner__track { fill: none; stroke: rgba(255,255,255,0.14); stroke-width: 2; }
.status-spinner__dash { fill: none; stroke: currentColor; stroke-linecap: round; stroke-width: 2; stroke-dasharray: 18 40; }

@keyframes conn-pulse {
  0% { transform: scale(1); opacity: 1; }
  50% { transform: scale(1.08); opacity: 0.92; }
  100% { transform: scale(1); opacity: 1; }
}

.conn-popup {
  position: absolute;
  top: calc(100% + 8px);
  right: 0;
  /* raise above common map and UI layers (e.g. leaflet panes) */
  z-index: 9999;
  min-width: 260px;
  max-width: 360px;
  background: var(--color-background);
  border: 1px solid var(--color-border);
  border-radius: 0.6rem;
  padding: 0.6rem;
  box-shadow: 0 10px 30px rgba(15,23,42,0.12);
}

.connection-list-compact { list-style: none; padding: 0; margin: 0; display: grid; gap: 0.5rem; }
.connection-card-compact { padding: 0.35rem; border-radius: 0.45rem; }
.connection-card-compact__row { display: flex; gap: 0.6rem; align-items: flex-start; }
.mini-status { width: 1.1rem; height: 1.1rem; display:inline-flex; align-items:center; justify-content:center; border-radius:999px; font-size:0.8rem; }
.mini-status--connected { background: #22c55e; color: white; }
.mini-status--loading { color: var(--color-border-hover); }
.mini-spinner { width: 1rem; height: 1rem; }
.mini-spinner__track { fill:none; stroke: var(--color-border); stroke-width:2 }
.mini-spinner__dash { fill:none; stroke: currentColor; stroke-linecap:round; stroke-width:2; stroke-dasharray: 12 30 }

.connection-card-compact__text { min-width: 0; }
.connection-card-compact__title-row { display:flex; align-items:center; justify-content:space-between; gap:0.5rem; }
.connection-card-compact__badge { font-size:0.72rem; padding:0.15rem 0.4rem; border-radius:999px; background: var(--color-background-mute); }

.subtask-list-compact { list-style:none; margin:0.3rem 0 0; padding:0; display:grid; gap:0.18rem }
.subtask-list-compact__item { display:flex; gap:0.4rem; align-items:center; font-size:0.82rem; color:var(--color-text); }
.subtask-dot { width:0.9rem; height:0.9rem; display:inline-flex; align-items:center; justify-content:center; border-radius:999px; font-size:0.72rem }
.subtask-dot--done { background:#d1fae5; color:#059669 }
.subtask-dot--loading { color:var(--color-border-hover); font-size: 1rem; }

@media (max-width: 480px) {
  .conn-popup { right: 0; left: 8px; min-width: auto; width: calc(100% - 16px); }
}

</style>

