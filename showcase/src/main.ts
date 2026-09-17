import { createApp } from 'vue'
import { createPinia } from 'pinia'
import { createRouter, createWebHistory } from 'vue-router'
import App from './App.vue'
import './styles/base.css'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  scrollBehavior: () => ({ top: 0 }),
  routes: [
    { path: '/', component: () => import('./pages/HomePage.vue') },
    { path: '/experience/scenes', component: () => import('./pages/ScenesPage.vue') },
    { path: '/experience/protocols', component: () => import('./pages/ProtocolsPage.vue') },
    { path: '/experience/monitor', component: () => import('./pages/MonitorPage.vue') },
    { path: '/experience/flows', component: () => import('./pages/FlowsPage.vue') },
  ],
})

createApp(App).use(createPinia()).use(router).mount('#app')
