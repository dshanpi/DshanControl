<script setup lang="ts">
withDefaults(defineProps<{ size?: number }>(), { size: 40 })
</script>

<template>
  <svg class="brand-svg" :width="size" :height="size" viewBox="0 0 48 48" role="img" aria-label="DshanControl">
    <defs>
      <linearGradient id="brand-a" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#2390ff"/><stop offset="1" stop-color="#075be7"/></linearGradient>
      <linearGradient id="brand-b" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#116fe9"/><stop offset="1" stop-color="#0747be"/></linearGradient>
      <linearGradient id="brand-c" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#52a9ff"/><stop offset="1" stop-color="#1772ee"/></linearGradient>
    </defs>
    <path fill="url(#brand-a)" d="M24 2 42 12.5v10.2l-9-5.2-9-5.2-9 5.2-9 5.2V12.5z"/>
    <path fill="url(#brand-b)" d="m6 18 9-5.2v20.8l9 5.2V49L6 38.5z"/>
    <path fill="url(#brand-c)" d="m42 18-9-5.2v20.8l-9 5.2V49l18-10.5z"/>
    <path fill="#fff" d="m24 14.5 8.2 4.7v9.6L24 33.5l-8.2-4.7v-9.6z" opacity=".94"/>
  </svg>
</template>

<style scoped>.brand-svg{display:block;filter:drop-shadow(0 4px 6px rgba(25,101,224,.18))}</style>
