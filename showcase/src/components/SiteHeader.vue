<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRoute } from 'vue-router'
import BrandMark from './BrandMark.vue'
import DIcon from './DIcon.vue'

const route = useRoute()
const mobileOpen = ref(false)
const nav = [
  { label: '解决方案', to: '/' },
  { label: '场景体验', to: '/experience/scenes' },
  { label: '协议接入', to: '/experience/protocols' },
  { label: '开发资源', href: 'https://github.com/dshanpi/DshanControl' },
]
const active = computed(() => route.path)
</script>

<template>
  <header class="site-header">
    <RouterLink class="brand" to="/" aria-label="DshanControl 首页">
      <BrandMark :size="40" />
      <span><b>Dshan<span>Control</span></b><small v-if="route.path === '/'" class="version">v0.1.0-dev</small><small v-else>连接设备　驱动可能</small></span>
    </RouterLink>
    <button class="mobile-toggle" aria-label="打开导航" @click="mobileOpen = !mobileOpen"><DIcon name="menu" :size="25" /></button>
    <nav :class="{ open: mobileOpen }">
      <template v-for="item in nav" :key="item.label">
        <a v-if="item.href" :href="item.href" target="_blank" rel="noreferrer">{{ item.label }}</a>
        <RouterLink v-else :class="{ active: active === item.to || (item.to === '/experience/scenes' && active === '/experience/flows') }" :to="item.to!" @click="mobileOpen = false">{{ item.label }}</RouterLink>
      </template>
    </nav>
    <div class="header-actions">
      <a class="github" href="https://github.com/dshanpi/DshanControl" target="_blank" rel="noreferrer"><DIcon name="github" :size="21" :stroke="2" /> GitHub</a>
      <RouterLink class="btn btn-primary header-demo" to="/experience/scenes">体验演示 <DIcon name="chevronRight" :size="17" /></RouterLink>
    </div>
  </header>
</template>

<style scoped>
.site-header { position: relative; z-index: 100; height: var(--header-h); padding: 0 max(72px, calc((100vw - 1528px)/2)); display: grid; grid-template-columns: 300px 1fr 300px; align-items: center; background: rgba(255,255,255,.97); border-bottom: 1px solid #e5edf7; box-shadow: 0 2px 8px rgba(31,73,125,.04); }
.brand { min-width: 0; display: flex; align-items: center; gap: 13px; color: var(--heading); }
.brand b { display: block; font-size: 24px; line-height: 1; letter-spacing: -.7px; }
.brand b span { color: #075fe8; }
.brand small { display: block; margin-top: 5px; color: #6e819c; font-size: 9px; letter-spacing: .28em; }
.brand small.version { display:inline-flex; width:auto; margin:0 0 0 9px; padding:3px 6px; border:1px solid #9eb8d7; border-radius:6px; vertical-align:3px; letter-spacing:0; color:#536c8b; font-weight:500; }
nav { justify-self: center; height: 100%; display: flex; align-items: stretch; gap: 58px; }
nav a { position: relative; display: flex; align-items: center; color: #14294a; font-size: 15px; font-weight: 650; }
nav a:hover, nav a.active { color: var(--primary); }
nav a.active::after { content: ''; position: absolute; left: 0; right: 0; bottom: 0; height: 3px; background: var(--primary); border-radius: 3px 3px 0 0; }
.header-actions { display: flex; justify-content: flex-end; align-items: center; gap: 16px; }
.github { display:flex; align-items:center; gap:7px; font-size: 15px; font-weight: 650; color: var(--heading); }
.header-demo { height: 44px; padding: 0 22px; }
.mobile-toggle { display: none; }
@media (max-width: 1100px) {
  .site-header { padding: 0 24px; grid-template-columns: 250px 1fr auto; }
  nav { gap: 28px; }
  .github { display: none; }
}
@media (max-width: 760px) {
  .site-header { position: sticky; top: 0; padding: 0 14px; grid-template-columns: 1fr auto; }
  .brand b { font-size: 20px; }
  .brand small { display: none; }
  .brand :deep(.brand-svg) { width:32px; height:32px; }
  .mobile-toggle { display: block; border: 0; background: transparent; font-size: 24px; color: var(--heading); }
  nav { position: absolute; top: 60px; left: 0; right: 0; height: auto; padding: 12px 18px 18px; display: none; flex-direction: column; gap: 0; background: #fff; box-shadow: 0 12px 28px rgba(20,42,77,.14); }
  nav.open { display: flex; }
  nav a { min-height: 46px; }
  nav a.active::after { left: 0; right: auto; width: 28px; }
  .header-actions { display: none; }
}
</style>
