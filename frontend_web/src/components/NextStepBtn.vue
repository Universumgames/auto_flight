<script setup lang="ts">
const props = defineProps<{
  disabled?: boolean
}>()

const emits = defineEmits(['next'])

const nextPress = () => {
  if (!props.disabled) emits('next')
}
</script>

<template>
  <a type="button" class="next-step-button" :class="disabled ? 'disabled' : ''" @click="nextPress"> Next Step </a>
</template>

<style scoped>
.next-step-button {
  padding: 0.5rem 1rem;
  border-radius: 0.8rem;
  color: var(--color-text-primary-action);
  background-color: var(--color-primary-action);
  margin-top: 1rem;
  cursor: pointer;
}

.next-step-button.disabled {
  background-color: var(--color-primary-action-disabled);
  color: var(--color-text-primary-action-disabled);
  cursor: not-allowed;
}
</style>
