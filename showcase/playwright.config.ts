import { defineConfig, devices } from '@playwright/test'

export default defineConfig({
  testDir: './tests',
  testMatch: '**/*.e2e.ts',
  use: { baseURL: 'http://127.0.0.1:5173', trace: 'retain-on-failure' },
  webServer: { command: 'npm run dev', url: 'http://127.0.0.1:5173', reuseExistingServer: true },
  projects: [{ name: 'desktop', use: { ...devices['Desktop Chrome'], viewport: { width: 1672, height: 941 } } }, { name: 'mobile', use: { ...devices['Desktop Chrome'], viewport: { width: 375, height: 812 } } }],
})
