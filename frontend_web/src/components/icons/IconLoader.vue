<script setup lang="ts"></script>

<template>
  <span class="status-icon status-icon--connected" aria-hidden="true">
    <svg class="status-spinner" viewBox="0 0 24 24" role="presentation" aria-hidden="true">
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
</template>

<style scoped>
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

.status-icon--loading {
  color: var(--color-border-hover);
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
  position: relative;
  top: 20%;
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
</style>
