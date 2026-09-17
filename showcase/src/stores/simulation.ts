import { computed, ref } from 'vue'
import { defineStore } from 'pinia'

export type ScenarioId = 'line' | 'energy' | 'edge'
export type DeviceState = 'online' | 'offline' | 'warning'

export interface Device {
  id: string
  name: string
  kind: 'gateway' | 'motor' | 'meter' | 'temperature' | 'display'
  protocol: string
  channel: string
  state: DeviceState
  value: number
  unit: string
}

const BASE_DEVICES: Device[] = [
  { id: 'meter-01', name: '电表 - 01', kind: 'meter', protocol: 'Modbus RTU', channel: 'RS485-01', state: 'online', value: 220.4, unit: 'V' },
  { id: 'motor-02', name: '电机 - 02', kind: 'motor', protocol: 'CANopen', channel: 'CAN-FD-01', state: 'online', value: 1480, unit: 'rpm' },
  { id: 'temp-03', name: '温度 - 03', kind: 'temperature', protocol: 'SOEM EtherCAT', channel: 'ECAT-01', state: 'warning', value: 52.6, unit: '°C' },
  { id: 'gateway-01', name: 'DshanControl 工业网关', kind: 'gateway', protocol: 'MQTT / API', channel: 'management0', state: 'online', value: 24, unit: '%' },
]

function round(value: number, digits = 1) {
  const scale = 10 ** digits
  return Math.round(value * scale) / scale
}

export const useSimulationStore = defineStore('simulation', () => {
  const devices = ref(BASE_DEVICES.map(item => ({ ...item })))
  const scenario = ref<ScenarioId>('line')
  const selectedDeviceId = ref('temp-03')
  const running = ref(true)
  const speed = ref(1)
  const tick = ref(0)
  const samples = ref<number[]>(Array.from({ length: 32 }, (_, i) => round(44 + i * .25 + Math.sin(i * .85) * 2)))
  let timer: number | undefined

  const selectedDevice = computed(() => devices.value.find(device => device.id === selectedDeviceId.value) ?? null)
  const temperature = computed(() => devices.value.find(device => device.id === 'temp-03')?.value ?? 0)
  const activeAlarms = computed(() => devices.value.filter(device => device.state === 'warning').length)
  const onlineCount = computed(() => devices.value.filter(device => device.state !== 'offline').length)

  function advance() {
    if (!running.value) return
    tick.value += 1
    const temp = devices.value.find(device => device.id === 'temp-03')!
    const motor = devices.value.find(device => device.id === 'motor-02')!
    const meter = devices.value.find(device => device.id === 'meter-01')!
    const phase = tick.value * .43
    temp.value = round(50.4 + Math.sin(phase) * 2.2)
    temp.state = temp.value > 50 ? 'warning' : 'online'
    motor.value = Math.round(1480 + Math.sin(phase * .7) * 28)
    meter.value = round(220.1 + Math.sin(phase * .5) * .45)
    samples.value = [...samples.value.slice(-31), temp.value]
  }

  function start() {
    if (timer) window.clearInterval(timer)
    timer = window.setInterval(advance, 1000 / speed.value)
  }
  function toggle() { running.value = !running.value }
  function setSpeed(value: number) { speed.value = value; start() }
  function select(id: string) { selectedDeviceId.value = id }
  function toggleOffline(id: string) {
    const device = devices.value.find(item => item.id === id)
    if (!device) return
    device.state = device.state === 'offline' ? (device.id === 'temp-03' && device.value > 50 ? 'warning' : 'online') : 'offline'
  }
  function reset() {
    devices.value = BASE_DEVICES.map(item => ({ ...item }))
    selectedDeviceId.value = 'temp-03'
    running.value = true
    speed.value = 1
    tick.value = 0
    samples.value = Array.from({ length: 32 }, (_, i) => round(44 + i * .25 + Math.sin(i * .85) * 2))
    start()
  }

  if (typeof window !== 'undefined') start()
  return { devices, scenario, selectedDeviceId, selectedDevice, running, speed, samples, temperature, activeAlarms, onlineCount, select, toggle, setSpeed, toggleOffline, reset }
})
