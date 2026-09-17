<script setup lang="ts">
import { computed } from 'vue'
import IndustrialScene from '../components/IndustrialScene.vue'
import Sparkline from '../components/Sparkline.vue'
import DIcon from '../components/DIcon.vue'
import { useSimulationStore, type ScenarioId } from '../stores/simulation'

const store = useSimulationStore()
const scenarios: { id: ScenarioId; label: string; icon: string }[] = [
  { id: 'line', label: '产线采集', icon: 'gateway' }, { id: 'energy', label: '能耗监测', icon: 'gauge' }, { id: 'edge', label: '边缘联动', icon: 'network' },
]
const selected = computed(() => store.selectedDevice ?? store.devices[2])
const history = computed(() => store.samples.slice(-18))
</script>

<template>
  <div class="scenes page">
    <section class="scene-head container">
      <div><h1>看见每一条数据的流向</h1><p>从现场设备到边缘应用，交互探索工业网关方案。</p></div>
      <div class="scene-intro"><span class="demo-badge"><i class="dot"></i>演示模式</span><p>连接设备 · 融合数据 · 驱动应用<br>从协议接入到上层应用的完整体验</p></div>
    </section>
    <section class="scene-layout container">
      <div class="scene-left">
        <div class="tab-row scenario-tabs">
          <button v-for="item in scenarios" :key="item.id" class="tab" :class="{ active: store.scenario === item.id }" @click="store.scenario = item.id"><DIcon :name="item.icon" :size="20" />{{ item.label }}</button>
        </div>
        <div class="scene-canvas surface">
          <IndustrialScene variant="topology" />
          <div class="scene-toolbar">
            <button class="pause" :aria-label="store.running ? '暂停' : '播放'" @click="store.toggle()"><DIcon :name="store.running ? 'pause' : 'play'" :size="17" :stroke="2.2" /></button>
            <span>演示速度</span>
            <select :value="store.speed" @change="store.setSpeed(Number(($event.target as HTMLSelectElement).value))"><option :value=".5">0.5×</option><option :value="1">1×</option><option :value="2">2×</option></select>
          </div>
          <div class="scene-legend"><span><i class="flow"></i>数据流</span><span><i></i>设备连接</span><span><i class="selected"></i>选中设备</span></div>
          <div class="zoom-controls"><button><DIcon name="zoomOut" :size="17" /></button><span>100%</span><button><DIcon name="zoomIn" :size="17" /></button><button title="重置视角" @click="store.reset()"><DIcon name="fullscreen" :size="17" /></button></div>
        </div>
      </div>
      <aside class="inspector surface">
        <header><b><DIcon name="chart" :size="18" />设备详情</b><DIcon name="chevronDown" :size="17" /></header>
        <div class="device-summary">
          <div class="device-thumb" :class="selected.kind"><DIcon :name="selected.kind === 'temperature' ? 'temperature' : selected.kind === 'motor' ? 'gauge' : 'meter'" :size="50" :stroke="1.4" /></div>
          <div><h2>温度传感器</h2><p><i class="dot"></i>在线 · 演示</p></div>
        </div>
        <dl>
          <dt>点位名称</dt><dd>{{ selected.kind === 'temperature' ? 'temperature' : selected.kind }}</dd>
          <dt>所属通道</dt><dd>RS485-01</dd>
          <dt>通信协议</dt><dd>Modbus RTU</dd>
          <dt>设备类型</dt><dd>温度传感器</dd>
          <dt>当前数值</dt><dd><b>36.8 °C</b></dd>
          <dt>更新时间</dt><dd>演示时钟 · 刚刚</dd>
        </dl>
        <h3>实时数据 <small>（最近 10 分钟）</small></h3>
        <div class="mini-chart"><span>50</span><span>25</span><span>0</span><Sparkline :values="history.map((value, index) => 25 + (value % 9) + index * .25)" fill/><b>36.8 °C</b></div>
        <div class="axis"><span>14:20</span><span>14:24</span><span>14:28</span></div>
        <div class="inspector-actions"><button class="btn btn-primary"><DIcon name="eye" :size="17" />查看点位</button><button class="btn" @click="store.toggleOffline(selected.id)"><DIcon :name="selected.state === 'offline' ? 'restart' : 'unplug'" :size="17" />{{ selected.state === 'offline' ? '恢复在线' : '模拟离线' }}</button></div>
      </aside>
    </section>
    <section class="journey container">
      <div><b>1</b><span class="journey-icon"><DIcon name="gateway" :size="25" /></span><p><strong>现场采集</strong><small>接入 CAN/CAN-FD、CANopen、RS485/Modbus RTU、SOEM EtherCAT 等多种工业设备。</small></p></div>
      <div><b>2</b><span class="journey-icon"><DIcon name="database" :size="25" /></span><p><strong>统一模型</strong><small>建立统一的设备与点位模型，实现数据标准化与灵活映射。</small></p></div>
      <div><b>3</b><span class="journey-icon"><DIcon name="monitor" :size="25" /></span><p><strong>应用订阅</strong><small>通过 API / MQTT 将数据推送至上层应用，支持 Qt HMI、Web 与 Enhanced。</small></p></div>
    </section>
    <footer class="page-footer"><span>DshanControl　让工业连接更简单</span><span>UI 概念设计 · 演示数据</span></footer>
  </div>
</template>

<style scoped>
.scenes { min-height: 873px; background: radial-gradient(circle at 65% 26%, #fff 0, #f1f7fd 44%, #eaf4fe 100%); }
.scene-head { height: 144px; padding-top: 26px; display: flex; align-items: flex-start; justify-content: space-between; }
.scene-head h1 { margin: 0 0 3px; color: #09244a; font-size: 37px; line-height: 1.3; letter-spacing: -.02em; }.scene-head > div > p { margin: 0; color: #526d8d; font-size: 19px; }
.scene-intro { text-align: right; padding-top: 1px; }.scene-intro p { margin: 19px 0 0; color: #8193aa; font-size: 13px; line-height: 1.9; }
.scene-layout { height: 542px; display: grid; grid-template-columns: minmax(0, 1fr) 378px; gap: 10px; }.scene-left { min-width: 0; }
.scenario-tabs { width: 620px; height: 44px; margin: 0 0 12px 2px; }.scenario-tabs .tab { display: flex; gap: 14px; align-items: center; justify-content: center; }
.scene-canvas { position: relative; height: 486px; overflow: hidden; }.scene-canvas :deep(.industrial-scene) { border-radius: 12px; }
.scene-toolbar, .scene-legend, .zoom-controls { position: absolute; z-index: 5; bottom: 10px; min-height: 38px; display: flex; align-items: center; border: 1px solid #dce7f3; border-radius: 9px; background: rgba(255,255,255,.92); box-shadow: 0 5px 16px rgba(38,79,128,.06); }
.scene-toolbar { left: 20px; padding: 3px 8px; gap: 10px; color: #57708f; font-size: 12px; }.scene-toolbar button { width: 34px; height: 30px; border: 0; border-radius: 7px; color: #fff; background: var(--primary); cursor: pointer; }.scene-toolbar select { height: 30px; border: 0; color: var(--heading); background: transparent; }
.scene-legend { left: 56%; transform: translateX(-50%); padding: 0 15px; gap: 22px; color: #70839f; font-size: 11px; }.scene-legend span { display: flex; align-items: center; gap: 7px; }.scene-legend i { width: 8px; height: 8px; border-radius: 50%; background: #8da8cb; }.scene-legend i.flow { background: var(--primary); }.scene-legend i.selected { width: 15px; height: 15px; border: 2px solid var(--primary); background: transparent; }
.zoom-controls { right: 16px; padding: 0 5px; }.zoom-controls button { width: 32px; height: 30px; border: 0; background: transparent; color: #24476f; font-size: 18px; }.zoom-controls span { min-width: 42px; text-align: center; color: #385674; font-size: 12px; }
.inspector { height: 530px; margin-top: 12px; padding: 0 16px 14px; }.inspector header { height: 48px; display: flex; align-items: center; justify-content: space-between; border-bottom: 1px solid #edf2f8; color: #17365c; }.inspector header b { display:flex; align-items:center; gap:8px; font-size: 15px; }
.device-summary { padding: 14px 0 9px; display: flex; gap: 15px; align-items: center; }.device-thumb { width: 112px; height: 92px; display: grid; place-items: center; flex: 0 0 auto; border-radius: 10px; background: linear-gradient(145deg,#f3f6f9,#e5ebf2); color: #617991; font-size: 50px; }.device-summary h2 { margin: 0 0 6px; color: #0d284d; font-size: 18px; }.device-summary p { margin: 0; color: var(--success); font-size: 12px; }.device-summary p.warning { color: var(--warning); }.device-summary p.offline { color: var(--offline); }.device-summary p i { display: inline-block; margin-right: 7px; }
dl { margin: 0; display: grid; grid-template-columns: 108px 1fr; font-size: 12px; line-height: 1.9; }dt { color: #788ba5; }dd { margin: 0; color: #314e70; }dd b { color: #17365b; }.inspector h3 { margin: 15px 0 5px; color: #19395e; font-size: 13px; }.inspector h3 small { color: #7186a1; font-weight: 400; }
.mini-chart { position: relative; height: 94px; padding: 4px 0 12px 28px; border-bottom: 1px solid #dce6f2; background: repeating-linear-gradient(to bottom, transparent, transparent 30px, #e8eef6 31px); }.mini-chart > span { position: absolute; left: 0; color: #7188a7; font-size: 10px; }.mini-chart > span:nth-child(1) { top: 2px; }.mini-chart > span:nth-child(2) { top: 33px; }.mini-chart > span:nth-child(3) { top: 66px; }.mini-chart .spark { height: 75px; }.mini-chart b { position: absolute; right: 0; top: 0; padding: 3px 7px; border-radius: 5px; color: #fff; background: var(--primary); font-size: 11px; }.axis { display: flex; justify-content: space-between; padding-left: 28px; color: #7187a3; font-size: 10px; }
.inspector-actions { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 17px; }.inspector-actions .btn { height: 43px; padding: 0 8px; font-size: 12px; }
.journey { height: 126px; padding-top: 18px; display: grid; grid-template-columns: repeat(3, 1fr); gap: 18px; }.journey > div { position: relative; min-width: 0; height: 107px; padding: 20px 20px 16px 72px; display: flex; align-items: flex-start; gap: 16px; border: 1px solid #e1eaf5; border-radius: 10px; background: rgba(255,255,255,.86); box-shadow: var(--shadow); }.journey > div > b { position: absolute; top: 15px; left: 20px; width: 34px; height: 34px; display: grid; place-items: center; color: #fff; border-radius: 50%; background: var(--primary); }.journey-icon { width: 46px; height: 46px; flex: 0 0 auto; display: grid; place-items: center; color: var(--primary); border-radius: 50%; background: #eaf3ff; font-size: 23px; }.journey strong, .journey small { display: block; }.journey strong { color: #17365a; font-size: 16px; }.journey small { margin-top: 4px; color: #687e9b; font-size: 11px; line-height: 1.6; }
@media (max-width: 1000px) { .scene-layout { height: auto; grid-template-columns: 1fr; }.scene-canvas { height: 500px; }.inspector { height: auto; margin: 0; }.journey { height: auto; grid-template-columns: 1fr; padding-bottom: 20px; }.scene-head { height: auto; padding-bottom: 20px; }.scene-intro p { display: none; } }
@media (max-width: 760px) { .scene-head { padding-top: 22px; }.scene-head h1 { font-size: 30px; }.scene-head > div > p { font-size: 15px; }.scene-intro { display: none; }.scenario-tabs { width: 100%; }.scene-canvas { height: 410px; }.scene-legend { display: none; }.zoom-controls { display: none; }.scene-layout { gap: 14px; }.inspector { padding-bottom: 18px; }.journey > div { height: auto; }.page-footer { display: none; } }
</style>
