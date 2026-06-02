<script setup lang="ts">
import { ConfigurationState, store } from '@/stores/store.ts'
import IconCheckmark from '@/components/icons/IconCheckmark.vue'
import IconLoader from '@/components/icons/IconLoader.vue'

defineProps<{
  name: string,
  state: ConfigurationState,
}>()
</script>

<template>
  <span
    >{{ name }}
    <IconCheckmark v-if="store.configurationState > state" />
    <IconLoader v-else-if="store.configurationState == state"
  /></span>
</template>

<style scoped></style>
