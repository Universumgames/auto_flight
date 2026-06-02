<script setup lang="ts"></script>

<template>
  <span class="status-icon status-icon--connected" aria-hidden="true">
    <span>✓</span>
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

.status-icon--connected {
  background: #22c55e;
  box-shadow: 0 0 0 0.2rem rgba(34, 197, 94, 0.14);
}
</style>
