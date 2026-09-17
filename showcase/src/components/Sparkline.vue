<script setup lang="ts">
import { computed } from 'vue'
const props = withDefaults(defineProps<{ values: number[]; color?: string; fill?: boolean }>(), { color: '#2676f3', fill: false })
const points = computed(() => {
  const min = Math.min(...props.values)
  const max = Math.max(...props.values)
  const span = max - min || 1
  return props.values.map((value, i) => `${(i / Math.max(1, props.values.length - 1)) * 100},${34 - ((value - min) / span) * 28}`).join(' ')
})
</script>

<template>
  <svg class="spark" viewBox="0 0 100 38" preserveAspectRatio="none" aria-hidden="true">
    <defs><linearGradient id="spark-fill" x1="0" y1="0" x2="0" y2="1"><stop offset="0" :stop-color="color" stop-opacity=".22"/><stop offset="1" :stop-color="color" stop-opacity="0"/></linearGradient></defs>
    <polygon v-if="fill" :points="`0,38 ${points} 100,38`" fill="url(#spark-fill)" />
    <polyline :points="points" fill="none" :stroke="color" stroke-width="1.8" vector-effect="non-scaling-stroke" />
  </svg>
</template>
