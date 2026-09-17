<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { VueFlow, useVueFlow, type Node, type Edge } from '@vue-flow/core'
import { Background } from '@vue-flow/background'
import { Controls } from '@vue-flow/controls'
import '@vue-flow/core/dist/style.css'
import '@vue-flow/core/dist/theme-default.css'
import '@vue-flow/controls/dist/style.css'
import { useSimulationStore } from '../stores/simulation'
import DIcon from '../components/DIcon.vue'

const store = useSimulationStore()
const running = ref(true)
const selectedNodeId = ref('threshold')
const threshold = ref(50)
const saved = ref(false)
const logs = ref<string[]>(['[10:24:17]  temperature: 52.6  ·  event: threshold_exceeded  ·  演示'])
const { onConnect, addEdges } = useVueFlow()
const nodes = ref<Node[]>([
  { id: 'mqtt', type: 'input', position: { x: 35, y: 115 }, data: { label: 'MQTT 输入', sub: 'topic: devices/+/points/+' }, class: 'node-mqtt' },
  { id: 'parse', position: { x: 275, y: 115 }, data: { label: '数据解析', sub: 'JSON 解析' }, class: 'node-parse' },
  { id: 'threshold', position: { x: 500, y: 115 }, data: { label: '阈值判断', sub: 'temperature > 50' }, class: 'node-threshold' },
  { id: 'trend', type: 'output', position: { x: 750, y: 50 }, data: { label: '趋势展示', sub: '图表可视化' }, class: 'node-trend' },
  { id: 'event', type: 'output', position: { x: 750, y: 180 }, data: { label: '事件记录', sub: '写入事件日志' }, class: 'node-event' },
])
const edges = ref<Edge[]>([
  { id: 'e1', source: 'mqtt', target: 'parse', animated: true }, { id: 'e2', source: 'parse', target: 'threshold', animated: true },
  { id: 'e3', source: 'threshold', target: 'trend', animated: true }, { id: 'e4', source: 'threshold', target: 'event', animated: true },
])
onConnect(connection => addEdges([connection]))
const selected = computed(() => nodes.value.find(node => node.id === selectedNodeId.value) ?? nodes.value[2])
function run() { running.value = !running.value; logs.value.unshift(`[演示时钟]  ${running.value ? '流程已启动' : '流程已停止'}  ·  ${edges.value.length} 条连接`) }
function reset() { threshold.value = 50; running.value = false; logs.value = ['[演示时钟]  流程已重置']; selectedNodeId.value = 'threshold' }
function save() { saved.value = true; localStorage.setItem('dshan-flow-demo', JSON.stringify({ threshold: threshold.value, nodes: nodes.value, edges: edges.value })); window.setTimeout(() => saved.value = false, 1500) }
watch(() => store.temperature, value => { if (running.value && value > threshold.value) logs.value = [`[演示时钟]  temperature: ${value.toFixed(1)}  ·  event: threshold_exceeded  ·  演示`, ...logs.value].slice(0, 8) })
</script>

<template>
  <div class="flows page">
    <div class="flow-layout">
      <aside class="flow-intro">
        <span class="eyebrow">概念交互 · 演示数据</span>
        <h1>让采集的数据，<br>驱动业务流程</h1>
        <p>基于 MQTT 数据契约，探索 Node-RED 扩展应用。</p>
        <div class="intro-steps">
          <div><b>1</b><span><DIcon name="mqtt" :size="23" /></span><p><strong>订阅标准数据</strong><small>通过 MQTT 订阅网关发布的统一设备与点位数据，快速获取现场实时数据。</small></p></div>
          <div><b>2</b><span><DIcon name="flow" :size="23" /></span><p><strong>组合处理逻辑</strong><small>使用可视化节点编排业务逻辑，进行数据解析、条件判断与联动处理。</small></p></div>
          <div><b>3</b><span><DIcon name="chart" :size="23" /></span><p><strong>输出业务结果</strong><small>将处理结果用于可视化展示、事件记录或系统集成，驱动实际业务流程。</small></p></div>
        </div>
        <div class="profile-tabs"><button>Lite</button><button class="active">Enhanced</button></div>
        <small class="profile-note">Node-RED 适用于 Enhanced。</small>
        <div class="feature-pills"><span><DIcon name="flow" :size="16" />可视化编排</span><span><DIcon name="database" :size="16" />统一数据契约</span></div>
        <div class="enhanced-note">Enhanced 主机配置已测试，目标板适配以仓库状态为准。</div>
      </aside>
      <main class="flow-main">
        <div class="flow-tabs"><button class="active"><DIcon name="flow" :size="17" />能耗监测示例</button><button><DIcon name="plus" :size="17" />新建示例</button><div><button class="run" @click="run"><DIcon :name="running ? 'stop' : 'play'" :size="16" />{{ running ? '停止演示' : '运行演示' }}</button><button @click="reset"><DIcon name="reset" :size="16" />重置</button><button class="icon-button" aria-label="更多"><DIcon name="more" :size="18" /></button></div></div>
        <section class="contract-banner"><div class="mini-gateway"><DIcon name="gateway" :size="38" :stroke="1.5" /></div><p><strong>边缘网关（数据源）</strong><small>采集现场设备数据，通过 MQTT 发布标准数据</small></p><div class="mqtt-line"><i></i><span>MQTT</span><DIcon name="chevronRight" :size="22" /></div><div class="cloud"><DIcon name="cloud" :size="35" :stroke="1.6" /></div><p><strong>网关标准数据</strong><small>devices/{id}/points/{name}<br>统一的设备与点位模型</small></p></section>
        <section class="flow-canvas">
          <VueFlow v-model:nodes="nodes" v-model:edges="edges" :min-zoom=".65" :max-zoom="1.4" :default-viewport="{ x: 0, y: 0, zoom: 1 }" @node-click="selectedNodeId = $event.node.id">
            <Background pattern-color="#c7d9ec" :gap="18" :size="1" />
            <Controls position="bottom-left" />
            <template #node-default="slotProps"><div class="custom-node" :class="{ selected: selectedNodeId === slotProps.id }"><b>{{ slotProps.data.label }}</b><small>{{ slotProps.id === 'threshold' ? `temperature > ${threshold}` : slotProps.data.sub }}</small><em :class="{ running }">● {{ running ? '运行中' : '已停止' }}</em></div></template>
            <template #node-input="slotProps"><div class="custom-node" :class="{ selected: selectedNodeId === slotProps.id }"><b>{{ slotProps.data.label }}</b><small>{{ slotProps.data.sub }}</small><em :class="{ running }">● {{ running ? '已连接' : '已停止' }}</em></div></template>
            <template #node-output="slotProps"><div class="custom-node" :class="{ selected: selectedNodeId === slotProps.id }"><b>{{ slotProps.data.label }}</b><small>{{ slotProps.data.sub }}</small><em :class="{ running }">● {{ running ? '运行中' : '已停止' }}</em></div></template>
          </VueFlow>
          <aside class="property-panel">
            <header>{{ selected.data.label }}<button @click="selectedNodeId = ''">×</button></header>
            <label>字段<select><option>temperature</option><option>current</option></select></label>
            <label>条件<select><option>大于（&gt;）</option><option>小于（&lt;）</option></select></label>
            <label>阈值<input v-model.number="threshold" type="number"></label>
            <button class="btn btn-primary" @click="save">{{ saved ? '已保存' : '保存演示配置' }}</button>
            <p><DIcon name="info" :size="14" />此为演示配置，用于展示业务逻辑编排的可能性。</p>
          </aside>
          <div class="debug"><header><span><DIcon name="chevronDown" :size="14" />Debug 控制台</span><button @click="logs=[]"><DIcon name="trash" :size="14" />清空</button></header><div v-for="(log,i) in logs.slice(0,2)" :key="i"><code>{{ log }}</code></div></div>
        </section>
        <section class="flow-cards"><article><span><DIcon name="mqtt" :size="27" /></span><p><strong>MQTT 契约接入</strong><small>基于统一的数据契约，通过 MQTT 接入网关的设备与点位数据，为上层应用提供标准、灵活的数据源。</small></p><a>了解更多<DIcon name="chevronRight" :size="14" /></a></article><article><span><DIcon name="sliders" :size="27" /></span><p><strong>数据与控制分层</strong><small>采集、处理、展示、控制各司其职，基于标准数据模型构建灵活的应用流程。</small></p><a>了解更多<DIcon name="chevronRight" :size="14" /></a></article></section>
      </main>
    </div>
    <footer class="page-footer"><span></span><span>UI 概念设计 · 演示数据</span></footer>
  </div>
</template>

<style scoped>
.flows { min-height: 873px; background: radial-gradient(circle at 60% 32%,#fff,#edf6ff 80%); }.flow-layout { min-height: 830px; display: grid; grid-template-columns: 410px 1fr; }.flow-intro { position: relative; padding: 43px 32px 20px 38px; background: linear-gradient(135deg,#f8fbff,#eaf4ff); border-right: 1px solid #dbe7f3; }.flow-intro .eyebrow { display: inline-flex; padding: 5px 12px; border-radius: 14px; background: #dcecff; }.flow-intro h1 { margin: 16px 0 9px; color: #09234a; font-size: 38px; line-height: 1.4; letter-spacing: -.02em; }.flow-intro > p { color: #536f91; font-size: 16px; }.intro-steps { margin-top: 28px; }.intro-steps > div { position: relative; min-height: 106px; display: grid; grid-template-columns: 34px 48px 1fr; gap: 10px; }.intro-steps > div:not(:last-child)::after { content:''; position:absolute; left:16px; top:35px; bottom:-2px; border-left:1px solid #a8c8ec; }.intro-steps b { z-index:1; width: 32px; height: 32px; display:grid; place-items:center; color:#fff; border-radius:50%; background:var(--primary); }.intro-steps > div > span { width: 44px; height:44px; display:grid; place-items:center; border-radius:50%; background:#e7f2ff; color:var(--primary); font-size:23px; }.intro-steps p { margin:0; }.intro-steps strong,.intro-steps small { display:block; }.intro-steps strong { color:#17365a; font-size:16px; }.intro-steps small { margin-top:5px; color:#657d99; font-size:11px; line-height:1.7; }.profile-tabs { height:48px; display:flex; margin-top:9px; border:1px solid #c8daee; border-radius:22px; overflow:hidden; }.profile-tabs button { flex:1; border:0; background:rgba(255,255,255,.42); color:#345373; }.profile-tabs button.active { color:#fff; background:var(--primary); border-radius:20px; font-weight:700; }.profile-note { display:block; margin-top:8px; color:#697e99; }.feature-pills { margin-top:37px; display:flex; gap:14px; }.feature-pills span { padding:11px 17px; border-radius:22px; background:#deecfc; color:#204568; font-size:12px; font-weight:650; }.enhanced-note { position:absolute; left:38px; bottom:27px; color:#7287a2; font-size:10px; }
.flow-main { min-width:0; padding:43px 32px 0 26px; }.flow-tabs { height:45px; display:flex; align-items:stretch; }.flow-tabs > button { min-width:205px; border:1px solid #dae5f1; border-radius:10px 10px 0 0; color:#6e829c; background:#f4f7fb; }.flow-tabs > button.active { color:var(--primary); background:#fff; border-bottom:3px solid var(--primary); font-weight:700; }.flow-tabs > div { margin-left:auto; display:flex; gap:12px; }.flow-tabs > div button { min-width:93px; padding:0 16px; border:1px solid #d4e0ed; border-radius:7px; background:#fff; color:#334f6f; }.flow-tabs > div .run { min-width:120px; color:#fff; border-color:var(--primary); background:var(--primary); }
.contract-banner { height:108px; padding:14px 45px; display:flex; align-items:center; gap:20px; border:1px solid #dce7f3; border-radius:11px 11px 0 0; background:linear-gradient(100deg,#f1f7fe,#eaf4ff); }.mini-gateway { width:82px; height:68px; display:grid; place-items:center; color:#fff; border-radius:5px; background:linear-gradient(135deg,#5d7285,#14283a); font-size:35px; box-shadow:8px 10px 15px rgba(23,58,91,.16); }.contract-banner p { margin:0; min-width:240px; }.contract-banner strong,.contract-banner small { display:block; }.contract-banner strong { color:#173659; font-size:14px; }.contract-banner small { margin-top:5px; color:#6b819c; font-size:10px; line-height:1.6; }.mqtt-line { min-width:210px; display:flex; align-items:center; gap:15px; color:#557493; }.mqtt-line i { flex:1; border-top:4px dotted #6ca7f5; }.mqtt-line span { font-size:11px; }.mqtt-line b { color:var(--primary); font-size:25px; }.cloud { width:82px; height:58px; display:grid; place-items:center; border-radius:50%; color:var(--primary); background:#d6eaff; font-size:32px; }
.flow-canvas { position:relative; height:448px; border:1px solid #dce7f3; border-top:0; background:#fff; overflow:hidden; }.flow-canvas :deep(.vue-flow) { position:absolute; left:0; right:210px; top:0; bottom:82px; }.flow-canvas :deep(.vue-flow__edge-path) { stroke:#2b83ed; stroke-width:2; }.flow-canvas :deep(.vue-flow__node) { width:166px; border:0; padding:0; background:transparent; box-shadow:none; }.custom-node { width:166px; min-height:62px; padding:12px 15px; border:1.5px solid #5295ed; border-radius:7px; background:#f7fbff; box-shadow:0 5px 13px rgba(22,93,179,.09); }.node-mqtt .custom-node { border-color:#b36cea; background:#fcf8ff; }.node-trend .custom-node { border-color:#4ab78d; background:#f1fcf7; }.node-event .custom-node { border-color:#ef9751; background:#fff8f2; }.custom-node.selected { border-width:2.5px; box-shadow:0 0 0 3px rgba(37,117,237,.12); }.custom-node b,.custom-node small,.custom-node em { display:block; }.custom-node b { color:#17375a; font-size:12px; }.custom-node small { margin-top:4px; color:#627b98; font-size:9px; }.custom-node em { position:absolute; left:18px; top:70px; color:#899bb4; font-size:9px; font-style:normal; }.custom-node em.running { color:var(--success); }
.property-panel { position:absolute; z-index:5; right:0; top:0; bottom:82px; width:210px; padding:0 14px; border-left:1px solid #dce7f3; background:#fff; }.property-panel header { height:44px; display:flex; align-items:center; justify-content:space-between; color:#173658; font-size:13px; font-weight:700; }.property-panel header button { border:0; background:transparent; font-size:20px; }.property-panel label { display:block; margin:8px 0; color:#435f7e; font-size:10px; }.property-panel select,.property-panel input { width:100%; height:34px; margin-top:5px; padding:0 9px; border:1px solid #d0ddea; border-radius:5px; color:#294867; background:#fff; }.property-panel .btn { width:100%; height:38px; margin-top:8px; padding:0; font-size:11px; }.property-panel p { margin-top:18px; padding:11px; border-radius:6px; color:#66809d; background:#eef6ff; font-size:9px; line-height:1.55; }
.debug { position:absolute; left:0; right:210px; bottom:0; height:82px; border-top:1px solid #dce7f3; background:#fff; overflow:hidden; }.debug header { height:36px; padding:0 14px; display:flex; align-items:center; justify-content:space-between; background:#f5f8fb; color:#3a5674; font-size:10px; }.debug header button { border:0; color:#72859d; background:transparent; }.debug div { padding:8px 15px; color:#506c8d; font-size:10px; }.debug code { color:#1768ed; }
.flow-cards { height:129px; margin-top:18px; display:grid; grid-template-columns:1fr 1fr; gap:18px; }.flow-cards article { position:relative; height:129px; padding:22px 90px 15px 100px; display:flex; align-items:flex-start; border:1px solid #dae6f2; border-radius:10px; background:rgba(255,255,255,.84); }.flow-cards article > span { position:absolute; left:24px; top:27px; width:55px; height:55px; display:grid; place-items:center; border-radius:50%; color:var(--primary); background:#e5f1ff; font-size:26px; }.flow-cards p { margin:0; }.flow-cards strong,.flow-cards small { display:block; }.flow-cards strong { color:#173659; font-size:16px; }.flow-cards small { margin-top:7px; color:#667e9a; font-size:10px; line-height:1.65; }.flow-cards a { position:absolute; right:20px; top:27px; color:var(--primary); font-size:11px; }
.feature-pills span,.flow-tabs button,.debug header span,.debug header button,.flow-cards a { display:flex; align-items:center; gap:7px; }
.flow-tabs button { justify-content:center; }
.flow-tabs > div .icon-button { min-width:42px; padding:0; }
.property-panel p { display:flex; align-items:flex-start; gap:6px; }
@media (max-width:1200px) { .flow-layout { grid-template-columns:310px 1fr; }.flow-intro { padding-left:24px; }.flow-intro h1 { font-size:32px; }.contract-banner { padding-inline:20px; }.mqtt-line { min-width:100px; }.contract-banner p { min-width:170px; } }
@media (max-width:760px) { .flow-layout { grid-template-columns:1fr; }.flow-intro { padding:28px 18px; }.flow-intro h1 { font-size:32px; }.enhanced-note { position:static; margin-top:35px; }.flow-main { padding:18px 12px; }.flow-tabs > button { min-width:170px; }.flow-tabs > button:nth-child(2) { display:none; }.flow-tabs > div button:not(.run) { display:none; }.contract-banner { height:auto; flex-wrap:wrap; }.mqtt-line { order:5; width:100%; }.flow-canvas { height:560px; overflow-x:auto; }.flow-canvas :deep(.vue-flow) { min-width:800px; right:0; bottom:170px; }.property-panel { top:auto; left:0; right:0; bottom:70px; width:100%; height:100px; display:flex; gap:10px; align-items:center; overflow-x:auto; }.property-panel header { min-width:100px; }.property-panel label { min-width:110px; }.property-panel .btn { min-width:130px; }.property-panel p { display:none; }.debug { right:0; height:70px; }.flow-cards { height:auto; grid-template-columns:1fr; }.flow-cards article { padding-right:20px; }.page-footer { display:none; } }
</style>
