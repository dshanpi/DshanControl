import { expect, test } from '@playwright/test'

const routes = [
  ['/', '工业现场互联'],
  ['/experience/scenes', '看见每一条数据的流向'],
  ['/experience/protocols', '多协议接入，一屏掌握'],
  ['/experience/monitor', '设备监控演示'],
  ['/experience/flows', '让采集的数据'],
] as const

for (const [path, title] of routes) {
  test(`${path} renders without horizontal overflow`, async ({ page }) => {
    await page.goto(path)
    await expect(page.getByText(title, { exact: false }).first()).toBeVisible()
    const overflow = await page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth)
    expect(overflow).toBeLessThanOrEqual(1)
  })
}
