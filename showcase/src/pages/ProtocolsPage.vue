<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import Sparkline from '../components/Sparkline.vue'
import DIcon from '../components/DIcon.vue'
import { useSimulationStore } from '../stores/simulation'

const route = useRoute()
const router = useRouter()
const store = useSimulationStore()
const protocol = ref(String(route.query.protocol || 'modbus'))
const baud = ref('9600')
const dropdownOpen = ref(true)
const reading = ref(false)
const readStatus = ref<'idle'|'success'|'error'>('success')
const bauds = ['1200','2400','4800','9600','19200','38400','57600','115200']
const tabs = [{ id: 'can', label: 'CAN / CANopen', icon: 'network' }, { id: 'modbus', label: 'RS485 / Modbus RTU', icon: 'file' }, { id: 'ethercat', label: 'EtherCAT', icon: 'share' }]
const voltage = computed(() => store.devices.find(item => item.id === 'meter-01')?.value ?? 220.4)
const current = computed(() => Math.round((3.22 + Math.sin(store.temperature) * .08) * 100) / 100)
function selectProtocol(id: string) { protocol.value = id; dropdownOpen.value = id === 'modbus'; router.replace({ query: { ...route.query, protocol: id } }) }
function read() { reading.value = true; readStatus.value = 'idle'; window.setTimeout(() => { reading.value = false; readStatus.value = Number(baud.value) < 2400 ? 'error' : 'success' }, 650) }
</script>

<template>
  <div class="protocols page">
    <section class="protocol-hero">
      <div class="container hero-inner">
        <div><h1>多协议接入，一屏掌握</h1><p>选择协议，体验从设备发现到点位采集的完整过程。</p></div>
        <div class="factory-line">连接工业现场<br><span>让数据创造价值</span></div>
        <RouterLink class="demo-link" to="/experience/scenes"><DIcon name="playCircle" :size="21" />交互演示</RouterLink>
      </div>
      <div class="container protocol-tabs">
        <button v-for="tab in tabs" :key="tab.id" :class="{ active: protocol === tab.id }" @click="selectProtocol(tab.id)"><DIcon :name="tab.icon" :size="21" />{{ tab.label }}</button>
      </div>
    </section>
    <section class="steps container">
      <div class="done"><b><DIcon name="check" :size="22" :stroke="2.2" /></b><p><strong>01　选择通道</strong><small>选择通信通道与基础参数</small></p></div><i></i>
      <div class="active"><b>02</b><p><strong>配置采集</strong><small>设置寄存器与采集方式</small></p></div><i></i>
      <div><b>03</b><p><strong>查看数据</strong><small>获取实时数据并映射到统一点位模型</small></p></div>
    </section>
    <section class="protocol-workspace container">
      <div class="config-panel surface">
        <header><b><DIcon :name="protocol === 'modbus' ? 'file' : protocol === 'can' ? 'network' : 'share'" :size="18" />{{ protocol === 'modbus' ? 'RS485 通道' : protocol === 'can' ? 'CAN-FD 通道' : 'EtherCAT 主站' }}</b><span><i class="dot"></i>通道可用</span></header>
        <template v-if="protocol === 'modbus'">
          <label>通道选择<select><option>RS485-01</option><option>RS485-02</option></select></label>
          <div class="two-fields">
            <label>波特率<div class="fake-select" @click="dropdownOpen = !dropdownOpen"><span>{{ baud }}</span><DIcon name="chevronDown" :size="15" /><div v-if="dropdownOpen" class="dropdown"><button v-for="item in bauds" :key="item" :class="{ active: baud === item }" @click.stop="baud = item; dropdownOpen = false">{{ item }}</button></div></div></label>
            <label>从站地址<input value="1" type="number" min="1" max="247"></label>
          </div>
          <label>功能码<select><option>03 · 读保持寄存器</option><option>04 · 读输入寄存器</option></select></label>
        </template>
        <template v-else-if="protocol === 'can'">
          <label>逻辑通道<select><option>CAN-FD-01</option><option>CAN-FD-02</option></select></label>
          <label>节点 ID<input value="2" type="number"></label>
          <label>对象字典<select><option>0x2000:01 · 转速</option></select></label>
        </template>
        <template v-else>
          <label>逻辑主站<select><option>ECAT-01 · SOEM</option></select></label>
          <label>从站<select><option>Slave 1 · IO Module</option></select></label>
          <label>PDO 映射<select><option>Temperature input</option></select></label>
        </template>
        <button class="btn btn-primary read-btn" @click="read">{{ reading ? '正在读取…' : '读取数据' }}</button>
        <small class="config-note"><DIcon name="info" :size="13" />演示配置，不发送至真实设备。</small>
      </div>
      <div class="protocol-visual surface">
        <div class="meter-device"><b>电能计量表</b><small>（{{ protocol === 'modbus' ? 'Modbus RTU' : protocol === 'can' ? 'CANopen' : 'EtherCAT' }} 从站）</small><div class="meter-box"><span>{{ voltage.toFixed(1) }}<small>V</small><br>{{ current.toFixed(2) }}<small>A</small></span></div></div>
        <div class="data-packets"><span>{{ protocol === 'modbus' ? '01　03　00　00　00　02　CRC' : protocol === 'can' ? '602　40　00　20　01　00' : 'PDO　0x6000:01' }}</span><i></i><span>01　04　08　9C　00　CE</span><b>请求 → 响应</b></div>
        <div class="gateway-device"><b>DshanControl</b><small>工业网关（演示）</small><div class="gateway-box"><i></i><i></i><i></i><em>DC</em></div></div>
        <div class="adapter">RS485 适配器</div><div class="wire"></div>
      </div>
      <div class="result-panel surface">
        <header><b><DIcon name="database" :size="18" />采集结果</b><span :class="readStatus"><i class="dot"></i>{{ reading ? '读取中' : readStatus === 'error' ? '请求超时 · 演示' : '读取成功 · 演示' }}</span></header>
        <div class="result-metrics"><div><span><DIcon name="power" :size="21" />电压</span><b>{{ readStatus === 'error' ? '—' : voltage.toFixed(1) }} <small>V</small></b></div><div><span><DIcon name="activity" :size="21" />电流</span><b>{{ readStatus === 'error' ? '—' : current.toFixed(2) }} <small>A</small></b></div></div>
        <div class="result-charts"><div><Sparkline :values="store.samples.map((v,i) => 220 + Math.sin(v+i)*2)" fill/><small>最近 1 分钟</small></div><div><Sparkline :values="store.samples.map((v,i) => 3.2 + Math.cos(v+i)*.2)" fill/><small>最近 1 分钟</small></div></div>
      </div>
    </section>
    <section class="mapping surface container">
      <h2><DIcon name="database" :size="19" />点位映射 <small>（基于统一设备与点位模型）</small></h2>
      <div class="map-table"><b>寄存器地址</b><b>点位名称</b><b>数据类型</b><b>单位</b><b>缩放系数</b><b>映射到标准点位</b><b>说明</b>
        <template v-for="(row,i) in [['40001','电压（Voltage）','UINT16','V','0.1','voltage','相电压，示例数据'],['40002','电流（Current）','UINT16','A','0.01','current','总电流，示例数据'],['40003','有功功率（Power）','UINT32','W','1','power','总有功功率，示例数据']]" :key="row[0]"><span :class="{ selected: i === 0 }"><i class="row-dot"></i>{{ row[0] }}</span><span :class="{ selected: i === 0 }">{{ row[1] }}</span><span :class="{ selected: i === 0 }">{{ row[2] }}</span><span :class="{ selected: i === 0 }">{{ row[3] }}</span><span :class="{ selected: i === 0 }">{{ row[4] }}</span><span :class="{ selected: i === 0 }"><DIcon name="chevronRight" :size="13" /><em>{{ row[5] }}</em></span><span :class="{ selected: i === 0 }">{{ row[6] }}</span></template>
      </div>
    </section>
    <footer class="page-footer"><span>DshanControl　 |　 面向工业现场的连接与数据能力</span><span class="footer-note"><DIcon name="info" :size="13" />协议适配能力与外部从站互操作需按设备验证。　　 UI 概念设计 · 演示数据</span></footer>
  </div>
</template>

<style scoped>
.protocols { min-height: 873px; background: linear-gradient(180deg,#eef7ff 0,#fff 25%,#f6f9fd 100%); }
.protocol-hero { height: 173px; background: radial-gradient(circle at 70% 35%, #fff, #edf6ff 72%); border-bottom: 1px solid #e2ebf4; }.hero-inner { height: 130px; display: flex; align-items: flex-start; justify-content: space-between; padding-top: 27px; position: relative; }.hero-inner h1 { margin: 0 0 3px; color: #09234a; font-size: 37px; line-height: 1.3; }.hero-inner p { margin: 0; color: #516d8e; font-size: 18px; }.factory-line { position: absolute; right: 190px; top: 34px; transform: rotate(-5deg); color: #627b99; font-family: cursive; font-size: 15px; line-height: 1.8; }.factory-line span { margin-left: 24px; }.demo-link { margin-top: 17px; width: 148px; height: 43px; display: flex; align-items:center; justify-content:center; gap:9px; border: 1px solid #96bdf0; border-radius: 22px; color: var(--primary); font-size: 14px; font-weight: 700; }
.protocol-tabs { height: 44px; display: flex; align-items: end; gap: 3px; }.protocol-tabs button { width: 273px; height: 44px; display:flex; align-items:center; justify-content:center; gap:13px; border: 1px solid #cfdeef; border-radius: 8px 8px 0 0; background: rgba(255,255,255,.56); color: #173458; font-weight: 700; cursor: pointer; }.protocol-tabs button.active { color: var(--primary); background: #fff; border-color: var(--primary); border-bottom: 3px solid #fff; }
.steps { height: 82px; display: grid; grid-template-columns: 230px 1fr 230px 1fr 320px; align-items: center; gap: 18px; }.steps > div { display: flex; gap: 16px; align-items: center; }.steps > i { height: 1px; background: #b9d2ee; }.steps b { width: 44px; height: 44px; flex: 0 0 auto; display: grid; place-items: center; border-radius: 50%; background: #e9eff6; color: #536e8d; }.steps .done b, .steps .active b { color: #fff; background: var(--primary); }.steps strong, .steps small { display: block; }.steps strong { color: #173557; font-size: 14px; }.steps small { margin-top: 3px; color: #6e829d; font-size: 11px; }
.protocol-workspace { height: 329px; display: grid; grid-template-columns: 392px minmax(480px,1fr) 437px; gap: 12px; }.config-panel, .result-panel { padding: 0 20px 15px; }.config-panel header, .result-panel header { height: 56px; display: flex; align-items: center; justify-content: space-between; color: #18365a; }.config-panel header span { color: var(--success); font-size: 11px; }.config-panel header span i, .result-panel header span i { display: inline-block; margin-right: 6px; }.config-panel label { min-height: 44px; display: grid; grid-template-columns: 105px 1fr; align-items: center; color: #304f72; font-size: 12px; }.config-panel input, .config-panel select, .fake-select { width: 100%; height: 36px; padding: 0 13px; border: 1px solid #cbd9e9; border-radius: 6px; color: #233f61; background: #fff; outline: 0; }.two-fields { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }.two-fields label { grid-template-columns: 105px 1fr; }.fake-select { position: relative; display: flex; align-items: center; justify-content: space-between; cursor: pointer; border-color: var(--primary); }.dropdown { position: absolute; z-index: 20; top: 33px; left: 0; right: 0; padding: 4px; border: 1px solid #cbd9e9; border-radius: 4px; background: #fff; box-shadow: 0 8px 18px rgba(20,57,104,.15); }.dropdown button { width: 100%; height: 27px; padding: 0 9px; border: 0; border-radius: 4px; text-align: left; background: #fff; color: #284664; cursor: pointer; }.dropdown button.active, .dropdown button:hover { background: #dcecff; color: #123967; }.read-btn { width: 100%; margin-top: 10px; }.config-note { display: block; margin-top: 9px; color: #8294aa; font-size: 10px; }
.protocol-visual { position: relative; overflow: hidden; background: radial-gradient(circle at 45% 50%,#fff,#edf6ff); }.meter-device, .gateway-device { position: absolute; text-align: center; color: #173454; }.meter-device { left: 35px; top: 17px; }.gateway-device { right: 42px; top: 17px; }.meter-device b, .meter-device small, .gateway-device b, .gateway-device small { display: block; }.meter-box { width: 125px; height: 178px; margin: 10px auto 0; padding-top: 52px; border-radius: 7px; background: linear-gradient(145deg,#f7fafc,#cbd5df); border: 1px solid #c1ccd7; box-shadow: 9px 12px 22px rgba(31,67,105,.15); }.meter-box span { display: inline-block; width: 94px; padding: 10px 3px; border: 3px inset #aab7c1; background: #dfe7df; color: #182a32; font-family: monospace; font-size: 19px; line-height: 1.25; }.meter-box small { display: inline; font-size: 10px; }.gateway-box { width: 120px; height: 175px; margin-top: 10px; padding: 45px 0 0 16px; border-radius: 5px; background: linear-gradient(135deg,#2d4053,#101b27); box-shadow: 10px 14px 23px rgba(15,37,61,.22); transform: skewY(-4deg); }.gateway-box i { display: block; width: 30px; height: 24px; margin-bottom: 4px; border: 2px solid #344b61; background: #071522; }.gateway-box em { position: absolute; right: 15px; top: 78px; color: #fff; font-style: normal; font-size: 10px; }.data-packets { position: absolute; left: 40%; top: 70px; width: 200px; color: #51708e; text-align: center; font-size: 10px; }.data-packets span { display: block; margin: 0 0 23px; padding: 5px; border-radius: 10px; background: #d7eaff; color: #1763c7; }.data-packets span:nth-of-type(2) { background: #ddf6e9; color: #298868; }.data-packets i { display: block; margin: -9px 0 10px; border-top: 6px dotted #438ef5; }.data-packets b { font-weight: 500; }.adapter { position: absolute; left: 48%; bottom: 54px; padding: 13px 19px; color: #fff; border-radius: 5px; background: #227ace; font-size: 10px; }.wire { position: absolute; left: 125px; right: 112px; bottom: 58px; height: 5px; border-radius: 3px; background: #176be1; }
.result-panel header span { padding: 7px 13px; border-radius: 14px; color: #128258; background: #e8f8f0; font-size: 11px; }.result-panel header span.error { color: var(--danger); background: #fff0ef; }.result-metrics { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }.result-metrics > div { height: 116px; padding: 20px; border: 1px solid #dce6f1; border-radius: 9px; background: linear-gradient(135deg,#f4f8fd,#fff); }.result-metrics span { display: block; color: #254568; font-size: 13px; }.result-metrics b { display: block; margin-top: 16px; color: #09244a; font-size: 32px; text-align: center; }.result-metrics small { font-size: 15px; }.result-charts { margin-top: 14px; display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }.result-charts > div { height: 75px; border-top: 1px solid #e0e9f3; }.result-charts .spark { height: 48px; }.result-charts small { display: block; text-align: right; color: #8596ac; font-size: 9px; }
.mapping { height: 219px; margin-top: 12px; padding: 14px 20px; }.mapping h2 { margin: 0 0 10px; color: #17375c; font-size: 16px; }.mapping h2 small { color: #657b97; font-size: 11px; font-weight: 500; }.map-table { display: grid; grid-template-columns: .9fr 1.25fr .8fr .5fr .8fr 1.45fr 1.35fr; border: 1px solid #dce7f2; border-radius: 8px; overflow: hidden; }.map-table b, .map-table span { min-width: 0; height: 36px; padding: 0 10px; display: flex; align-items: center; border-bottom: 1px solid #e5edf5; color: #3a5675; font-size: 11px; }.map-table b { height: 34px; background: #f4f7fb; font-weight: 650; }.map-table span.selected { background: #dcecff; color: #244b76; }.map-table em { padding: 3px 10px; border: 1px solid #c7dcef; border-radius: 5px; background: rgba(255,255,255,.55); font-style: normal; flex: 1; }
.config-panel header b,.result-panel header b,.mapping h2,.config-note,.result-metrics span { display:flex; align-items:center; gap:8px; }
.footer-note { display:flex; align-items:center; gap:6px; }.row-dot { width:7px; height:7px; margin-right:8px; border:2px solid #6e89a6; border-radius:50%; }
@media (max-width: 1200px) { .protocol-workspace { grid-template-columns: 340px 1fr; height: auto; }.result-panel { grid-column: 1 / -1; }.mapping { height: auto; overflow-x: auto; }.map-table { min-width: 900px; }.factory-line { display: none; } }
@media (max-width: 760px) { .protocol-hero { height: auto; }.hero-inner { height: auto; padding: 24px 0; }.hero-inner h1 { font-size: 30px; }.hero-inner p { font-size: 15px; }.demo-link { display: none; }.protocol-tabs { overflow-x: auto; }.protocol-tabs button { min-width: 210px; }.steps { grid-template-columns: 1fr 1fr 1fr; gap: 7px; }.steps > i { display: none; }.steps b { width: 33px; height: 33px; }.steps small { display: none; }.protocol-workspace { grid-template-columns: 1fr; }.protocol-visual { height: 330px; }.result-panel { grid-column: auto; }.two-fields { grid-template-columns: 1fr; gap: 0; }.mapping { margin-bottom: 20px; }.page-footer { display: none; } }
</style>
