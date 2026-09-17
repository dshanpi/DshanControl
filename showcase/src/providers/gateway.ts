export type DataSource = 'demo' | 'live' | 'replay'
export type Quality = 'good' | 'stale' | 'bad' | 'unknown'
export type ConnectionState = 'connecting' | 'online' | 'degraded' | 'offline' | 'unknown'

export interface GatewayDevice {
  id: string
  name: string
  kind: 'gateway' | 'motor' | 'meter' | 'temperature' | 'io' | 'display'
  protocol: string
  source: DataSource
  connection: ConnectionState
  capabilities: string[]
}

export interface PointSample {
  pointId: string
  deviceId: string
  value: number | string | boolean | null
  unit?: string
  quality: Quality
  source: DataSource
  observedAt: string
  receivedAt: string
  sequence?: number
}

export interface HistoryQuery { pointId: string; from: string; to: string; limit?: number }
export interface GatewayCommand { deviceId: string; action: string; parameters?: Record<string, unknown> }

export interface GatewayProvider {
  connect(): Promise<void>
  disconnect(): Promise<void>
  getCapabilities(): Promise<Record<string, unknown>>
  listDevices(): Promise<GatewayDevice[]>
  subscribeSamples(handler: (sample: PointSample) => void): () => void
  queryHistory(query: HistoryQuery): Promise<PointSample[]>
  execute(command: GatewayCommand): Promise<{ commandId: string }>
}

export class DemoProvider implements GatewayProvider {
  private subscribers = new Set<(sample: PointSample) => void>()
  private sequence = 0
  private timer?: number
  async connect() {
    this.timer = window.setInterval(() => {
      this.sequence += 1
      const value = Math.round((50.4 + Math.sin(this.sequence * .43) * 2.2) * 10) / 10
      const time = new Date(Date.UTC(2026, 0, 1, 10, 24, 17 + this.sequence)).toISOString()
      const sample: PointSample = { pointId: 'temp-03.temperature', deviceId: 'temp-03', value, unit: '°C', quality: 'good', source: 'demo', observedAt: time, receivedAt: time, sequence: this.sequence }
      this.subscribers.forEach(handler => handler(sample))
    }, 1000)
  }
  async disconnect() { if (this.timer) window.clearInterval(this.timer); this.timer = undefined }
  async getCapabilities() { return { scenarios: ['line', 'energy', 'edge'], writable: false } }
  async listDevices(): Promise<GatewayDevice[]> { return [{ id: 'temp-03', name: '温度 - 03', kind: 'temperature', protocol: 'Modbus RTU', source: 'demo', connection: 'online', capabilities: ['read'] }] }
  subscribeSamples(handler: (sample: PointSample) => void) { this.subscribers.add(handler); return () => this.subscribers.delete(handler) }
  async queryHistory(query: HistoryQuery): Promise<PointSample[]> { return Array.from({ length: Math.min(query.limit ?? 30, 3600) }, (_, i) => ({ pointId: query.pointId, deviceId: 'temp-03', value: 44 + i * .2 + Math.sin(i) * 2, unit: '°C', quality: 'good' as const, source: 'demo' as const, observedAt: new Date(Date.UTC(2026,0,1,10,0,i)).toISOString(), receivedAt: new Date(Date.UTC(2026,0,1,10,0,i)).toISOString(), sequence: i })) }
  async execute(command: GatewayCommand) { return { commandId: `demo-${command.deviceId}-${++this.sequence}` } }
}

export class LiveProvider implements GatewayProvider {
  private stopped = false
  async request(path: string, init?: RequestInit) {
    const response = await fetch(path, { credentials: 'same-origin', ...init })
    if (!response.ok) throw new Error(response.status === 401 ? '需要登录后访问实机数据' : `实机接口错误 ${response.status}`)
    return response.json()
  }
  async connect() { this.stopped = false; await this.request('/api/v1/platform') }
  async disconnect() { this.stopped = true }
  async getCapabilities() { return this.request('/api/v1/platform') }
  async listDevices() { const data = await this.request('/api/v1/devices'); return (data.devices ?? []) as GatewayDevice[] }
  subscribeSamples(_handler: (sample: PointSample) => void) { return () => { this.stopped = true } }
  async queryHistory(query: HistoryQuery) { const params = new URLSearchParams(query as unknown as Record<string,string>); const data = await this.request(`/api/hmi/v1/history?${params}`); return (data.samples ?? []) as PointSample[] }
  async execute(command: GatewayCommand) { return this.request('/api/hmi/v1/actions', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(command) }) }
}

export class ReplayProvider extends DemoProvider {
  constructor(private samples: PointSample[]) { super() }
  override async queryHistory(_query: HistoryQuery): Promise<PointSample[]> { return this.samples }
}
