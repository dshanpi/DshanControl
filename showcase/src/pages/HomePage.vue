<script setup lang="ts">
import { ref } from 'vue'
import IndustrialScene from '../components/IndustrialScene.vue'
import DIcon from '../components/DIcon.vue'

const ready = ref(false)
const protocols = [
  { mark: 'CAN', title: 'CAN', sub: 'Controller Area Network', to: '/experience/protocols?protocol=can', icon: 'network' },
  { mark: 'CANopen', title: 'CANopen', sub: 'Device Profile for Industrial Automation', to: '/experience/protocols?protocol=canopen' },
  { mark: '', title: 'Modbus RTU', sub: 'Remote Terminal Unit', to: '/experience/protocols?protocol=modbus', icon: 'share' },
  { mark: 'EtherCAT', title: 'EtherCAT', sub: 'SOEM (Simple Open EtherCAT Master)', to: '/experience/protocols?protocol=ethercat' },
  { mark: '', title: 'MQTT', sub: '轻量级发布 / 订阅协议', to: '/experience/flows', icon: 'mqtt' },
]
const abilities = [
  { type: 'devices', icon: 'share', title: '设备接入', text: '支持 CAN / CANopen、RS485 / Modbus RTU、EtherCAT 等多种工业协议，快速接入现场设备。', to: '/experience/protocols' },
  { type: 'gateway', icon: 'database', title: '边缘处理', text: '统一数据模型，边缘采集、数据处理与协议转换，支持本地与远程协同。', to: '/experience/flows' },
  { type: 'screens', icon: 'chart', title: '应用展示', text: '提供 Qt HMI 与 Web 管理界面，支持 API / MQTT 数据对接与增强开发。', to: '/experience/monitor' },
]
</script>

<template>
  <div class="home page">
    <section class="hero">
      <div class="hero-copy">
        <div class="overline">OPEN SOURCE INDUSTRIAL PLATFORM</div>
        <h1>工业现场互联<br><span>从设备到数据，一站打通</span></h1>
        <p>统一设备接入、边缘采集与可视化管理。</p>
        <div class="tags"><span>T153MX</span><span>Qt HMI</span><span>Web 管理</span></div>
        <div class="hero-actions">
          <a class="btn btn-primary" href="#abilities">探索解决方案 <DIcon name="chevronRight" :size="18" /></a>
          <RouterLink class="btn play" to="/experience/scenes"><b><DIcon name="play" :size="14" /></b> 在线体验</RouterLink>
        </div>
        <div class="hero-features">
          <div><b><DIcon name="share" :size="27" /></b><span><strong>多协议接入</strong><small>主流工业协议，灵活扩展</small></span></div>
          <div><b><DIcon name="database" :size="27" /></b><span><strong>统一数据模型</strong><small>设备、点位、数据一致管理</small></span></div>
          <div><b><DIcon name="cloud" :size="28" /></b><span><strong>本地与远程协同</strong><small>从现场到云端，平滑打通</small></span></div>
        </div>
      </div>
      <div class="hero-visual" :class="{ ready }">
        <IndustrialScene variant="hero" @ready="ready = true" />
      </div>
    </section>

    <section class="protocol-strip">
      <RouterLink v-for="protocol in protocols" :key="protocol.title" :to="protocol.to">
        <b v-if="protocol.mark" :class="{ wordmark: protocol.mark.length > 3 }">{{ protocol.mark }}</b><DIcon v-else :name="protocol.icon || 'network'" :size="30" :stroke="2" />
        <span><strong>{{ protocol.title }}</strong><small>{{ protocol.sub }}</small></span>
      </RouterLink>
    </section>

    <section id="abilities" class="abilities">
      <h2>一套平台，连接完整工业现场</h2>
      <p>从设备接入，到边缘处理，再到多端应用展示，构建开放、灵活、可扩展的工业互联平台。</p>
      <div class="ability-grid">
        <RouterLink v-for="ability in abilities" :key="ability.title" class="ability-card" :class="ability.type" :to="ability.to">
          <div class="asset-illustration"><span><DIcon :name="ability.icon" :size="34" /></span><i></i><i></i><i></i></div>
          <div><h3><em><DIcon :name="ability.icon" :size="20" /></em>{{ ability.title }}</h3><p>{{ ability.text }}</p></div>
          <b class="arrow"><DIcon name="chevronRight" :size="18" /></b>
        </RouterLink>
      </div>
    </section>
    <footer class="page-footer"><span>〔 DshanControl　让工业互联更简单　·　开放　·　灵活</span><span>文档　 |　 社区　 |　 GitHub　　 UI 概念设计 · 演示数据</span></footer>
  </div>
</template>

<style scoped>
.home { background: #fff; }
.hero { position: relative; height: 485px; display: grid; grid-template-columns: 42% 58%; background: radial-gradient(circle at 62% 25%, #fff 0, #eef7ff 43%, #e9f4ff 100%); }
.hero-copy { z-index: 4; padding: 42px 0 0 max(80px, calc((100vw - 1512px)/2)); }
.overline { margin-bottom: 13px; color: #38577f; font-size: 12px; letter-spacing: .28em; }
h1 { margin: 0; color: #09244a; font-size: clamp(43px, 3.1vw, 52px); line-height: 1.17; letter-spacing: -.035em; font-weight: 850; }
h1 span { white-space: nowrap; }
.hero-copy > p { margin: 9px 0 15px; color: #24466f; font-size: 21px; letter-spacing: .04em; }
.tags { display: flex; gap: 16px; }
.tags span { min-width: 94px; height: 33px; display: grid; place-items: center; border: 1px solid #b8d5f4; border-radius: 7px; background: rgba(255,255,255,.5); color: #18395f; font-size: 13px; font-weight: 650; }
.hero-actions { display: flex; gap: 16px; margin-top: 20px; }
.hero-actions .btn { height: 51px; min-width: 184px; font-size: 16px; }
.hero-actions .play { border-color: var(--primary); color: #153863; }
.hero-actions .play b { width: 28px; height: 28px; display: grid; place-items: center; padding-left: 2px; border: 1px solid var(--primary); border-radius: 50%; color: var(--primary); font-size: 11px; }
.hero-features { margin-top: 32px; width: 560px; display: grid; grid-template-columns: repeat(3, 1fr); }
.hero-features > div { display: flex; align-items: center; gap: 12px; min-width: 0; padding-right: 18px; border-right: 1px solid #cad8e8; }
.hero-features > div + div { padding-left: 18px; }.hero-features > div:last-child { border-right: 0; }
.hero-features b { color: var(--primary); font-size: 25px; }.hero-features strong, .hero-features small { display: block; white-space: nowrap; }
.hero-features strong { color: #17395f; font-size: 14px; }.hero-features small { margin-top: 2px; color: #6f83a0; font-size: 10px; }
.hero-visual { position: relative; min-width: 0; height: 485px; opacity: .01; transition: opacity .35s ease; }
.hero-visual.ready { opacity: 1; }.hero-visual .demo-badge { position: absolute; z-index: 5; top: 20px; right: 34px; color: #163e35; background: rgba(255,255,255,.88); border-color: #b9cfeb; }
.protocol-strip { height: 86px; display: grid; grid-template-columns: repeat(5, 1fr); padding: 14px max(80px, calc((100vw - 1512px)/2)); border-top: 1px solid #e2ebf5; border-bottom: 1px solid #e2ebf5; background: #fff; box-shadow: 0 4px 14px rgba(45,82,126,.035); }
.protocol-strip a { min-width: 0; display: flex; align-items: center; justify-content: center; gap: 14px; border-right: 1px solid #d7e0eb; transition: background .15s; }
.protocol-strip a:last-child { border: 0; }.protocol-strip a:hover { background: #f6faff; }
.protocol-strip a > b { color: #1268ed; font-size: 30px; }.protocol-strip a > b.wordmark { color: #087c72; font-size: 23px; font-style: italic; }
.protocol-strip strong, .protocol-strip small { display: block; }.protocol-strip strong { color: #193557; font-size: 15px; }.protocol-strip small { margin-top: 2px; color: #7387a3; font-size: 10px; white-space: nowrap; }
.abilities { height: 270px; padding: 24px max(60px, calc((100vw - 1552px)/2)) 10px; background: linear-gradient(#fff, #fbfdff); text-align: center; }
.abilities h2 { margin: 0 0 3px; color: #0b2b52; font-size: 30px; letter-spacing: .02em; }.abilities > p { margin-bottom: 14px; color: #7085a1; font-size: 13px; }
.ability-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 18px; }
.ability-card { position: relative; height: 145px; padding: 18px 48px 16px 198px; display: flex; align-items: center; text-align: left; border: 1px solid #e1ebf6; border-radius: 12px; overflow: hidden; background: linear-gradient(120deg,#eef7ff,#fbfdff); }
.ability-card > div:nth-child(2) { min-width: 0; }
.ability-card.gateway { background: linear-gradient(120deg,#f0fbf7,#fbfffe); }.ability-card.screens { background: linear-gradient(120deg,#f2f6ff,#fbfcff); }
.asset-illustration { position: absolute; left: 22px; bottom: 0; width: 155px; height: 120px; display: grid; place-items: center; perspective: 400px; }
.asset-illustration span { position: relative; z-index: 2; width: 92px; height: 64px; display: grid; place-items: center; color: #fff; font-size: 33px; border-radius: 5px; background: linear-gradient(150deg,#526b83,#17293d); transform: rotateY(-18deg) rotateX(8deg); box-shadow: 14px 16px 22px rgba(33,70,108,.18); }
.asset-illustration i { position: absolute; bottom: 15px; width: 96px; height: 8px; border-radius: 50%; background: rgba(29,66,108,.16); filter: blur(4px); }
.ability-card h3 { margin: 0 0 7px; color: #17365b; font-size: 16px; }.ability-card h3 em { display: inline-grid; place-items: center; width: 32px; height: 32px; margin-right: 9px; border-radius: 7px; background: var(--primary); color: #fff; font-style: normal; }
.ability-card p { margin: 0; color: #617995; font-size: 12px; line-height: 1.65; overflow-wrap: anywhere; }.arrow { position: absolute; right: 18px; width: 36px; height: 36px; display: grid; place-items: center; border: 1px solid #aac9ef; border-radius: 50%; color: var(--primary); }
@media (max-width: 1100px) { .hero { grid-template-columns: 52% 48%; }.hero-copy { padding-left: 30px; }.hero-features { width: 500px; }.ability-card { padding-left: 135px; }.asset-illustration { width: 110px; }.protocol-strip { padding-inline: 24px; } }
@media (max-width: 760px) {
  .hero { height: auto; grid-template-columns: 1fr; }.hero-copy { padding: 40px 22px 22px; }.hero h1 { font-size: 36px; }.hero h1 span { white-space: normal; }.hero-copy > p { font-size: 17px; }.hero-actions { flex-wrap: wrap; }.hero-features { width: 100%; grid-template-columns: 1fr; gap: 12px; }.hero-features > div { border: 0; padding: 0 !important; }.hero-visual { height: 330px; }
  .protocol-strip { height: auto; padding: 8px 14px; grid-template-columns: 1fr 1fr; }.protocol-strip a { min-width: 0; min-height: 68px; border-bottom: 1px solid var(--border); }.protocol-strip a:nth-child(even) { border-right: 0; }.protocol-strip small { display: none; }.protocol-strip a > b { font-size: 23px; }.protocol-strip a > b.wordmark { font-size: 17px; }
  .abilities { height: auto; padding: 30px 14px; }.abilities h2 { font-size: 25px; }.ability-grid { grid-template-columns: 1fr; }.ability-card { padding-left: 122px; padding-right: 35px; }.asset-illustration { left: 8px; width: 100px; }.arrow { right: 8px; width: 28px; height: 28px; }.page-footer { display: none; }
}
</style>
