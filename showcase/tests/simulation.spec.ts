import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useSimulationStore } from '../src/stores/simulation'

describe('shared demonstration state', () => {
  beforeEach(() => setActivePinia(createPinia()))
  it('starts in the frozen reference state', () => {
    const store = useSimulationStore()
    expect(store.selectedDeviceId).toBe('temp-03')
    expect(store.temperature).toBe(52.6)
    expect(store.activeAlarms).toBe(1)
  })
  it('keeps offline state separate from the point value', () => {
    const store = useSimulationStore()
    store.toggleOffline('temp-03')
    expect(store.devices.find(device => device.id === 'temp-03')?.state).toBe('offline')
    expect(store.temperature).toBe(52.6)
  })
})
