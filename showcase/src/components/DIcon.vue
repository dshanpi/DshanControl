<script setup lang="ts">
import { computed, type Component } from 'vue'
import {
  Activity, Bell, Box, Cable, ChartNoAxesCombined, Check, ChevronDown,
  ChevronRight, CircleGauge, CirclePlay, Cloud, CodeXml, Cpu, Database,
  Eye, FileJson, Fullscreen, Gauge, GitFork, HardDrive, House, Info, Link,
  ListRestart, Maximize, Menu, Monitor, MoreVertical, Network, Pause,
  Play, Plus, RadioTower, RotateCcw, Save, Server, Settings2, Share2,
  SlidersHorizontal, Square, TableProperties, Thermometer, Unplug,
  Trash2, Workflow, Zap, ZoomIn, ZoomOut, Clock3,
} from '@lucide/vue'

const props = withDefaults(defineProps<{ name: string; size?: number; stroke?: number }>(), { size: 20, stroke: 1.8 })
const icons: Record<string, Component> = {
  activity: Activity, alarm: Bell, box: Box, cable: Cable, chart: ChartNoAxesCombined,
  check: Check, chevronDown: ChevronDown, chevronRight: ChevronRight, gauge: CircleGauge,
  playCircle: CirclePlay, cloud: Cloud, code: CodeXml, cpu: Cpu, database: Database,
  eye: Eye, file: FileJson, fullscreen: Fullscreen, meter: Gauge, github: GitFork,
  storage: HardDrive, home: House, info: Info, link: Link, restart: ListRestart, maximize: Maximize,
  menu: Menu, monitor: Monitor, more: MoreVertical, network: Network, pause: Pause,
  play: Play, plus: Plus, mqtt: RadioTower, reset: RotateCcw, save: Save, gateway: Server,
  settings: Settings2, share: Share2, sliders: SlidersHorizontal, stop: Square,
  table: TableProperties, temperature: Thermometer, unplug: Unplug, flow: Workflow,
  power: Zap, trash: Trash2, clock: Clock3, zoomIn: ZoomIn, zoomOut: ZoomOut,
}
const icon = computed(() => icons[props.name] ?? Box)
</script>

<template><component :is="icon" :size="size" :stroke-width="stroke" aria-hidden="true" /></template>
