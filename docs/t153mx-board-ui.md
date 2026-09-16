# T153MX 实板桌面

完整图库位于 README 顶部。以下文件均来自同一块 T153MX 的真实 framebuffer：

| 页面 | 文件 | SHA-256 |
| --- | --- | --- |
| 运行总览 | `01-overview-opaque.png` | `d62a2b1e895d0dfe5f1a4bdcdb304996f04c8d14e0949b8977b0d948701e396a` |
| CAN / CANopen | `02a-fieldbus-can-opaque.png` | `f43603b581164f174c936a80b194c92c8bb947d2745d25962a3bc96b97594ce2` |
| RS485 / Modbus | `02b-fieldbus-modbus-opaque.png` | `0c84c758fcd7276b7c65576221868c8f49df7fcc8093e60823241f5c9c6c5ad1` |
| EtherCAT | `02c-fieldbus-ethercat-opaque.png` | `1a5a6a1a3a060dc0602d54b4c755b8b31904e144bd28bcfae36a2d6100ed10c8` |
| 网络通信 | `03-network-opaque.png` | `6962329154a1eff9a9f9eecaccf277d5bbc9b324c9c611ac1950e00eeba89a7c` |
| 设备监控 | `04-devices-opaque.png` | `006a56e101fbc38f6de8ee8130d11af36465cf56c85843d4d147e776c8ea4b38` |
| 告警记录 | `05-alarms-opaque.png` | `15c85b3ed78625b0dd25f87343643d024c724d5e5a1920a467619bdf497d0ca0` |
| 系统设置 | `06-settings-opaque.png` | `014dc9f8b97bf9bbb908f9c0fe55964945db9f141da6446470ddbd551de91665` |

## 证据来源

- 采集时间：2026-09-16 11:26–11:27 UTC
- 采集方式：板端 `fbgrab` 直接读取真实 framebuffer
- 传输方式：板卡通过 Wi-Fi `wlan0` 获取地址后，由临时只读 HTTP 服务传输；
  板端与本地逐图 SHA-256 一致
- 发布处理：`fbgrab` 输出的 PNG 将 Alpha 错误写为全透明；发布文件仅移除该 Alpha
  通道并保留原始 RGB 像素，转换前后 RGB 字节已逐图校验一致；表中为发布文件 SHA-256
- 运行内核：`Linux Tina5.0 5.10.198 #69 SMP PREEMPT`
- 镜像大小：`604371968` bytes
- 镜像 SHA-256：`84914b6b79c59c5dd5221ba9aa59ae107c06d4826d193c10260ed7ce5cc87f17`
- 烧写结果：`partition + verify + reboot` 成功，Boot0/Boot1 verified，
  `verifyState=success`、`verifyErrorCode=0`
- 启动检查：已消除缺失 `powerkey_display` / `powerkey_suspend` 和 NFS server
  的启动告警；冷启动日志未见 panic、Oops 或 OOM。

## 界面检查

- 顶部显示 CPU、内存、温度和存储；底部不重复这些指标。
- 使用统一深灰色体系，黄色用于状态线条和重点边框。
- 8 张 1024×768 截图均可完整解码，未见文字截断、控件重叠或蓝白主题混用。
- 默认非演示模式；无外部从站时显示真实的停止、离线或未知状态。
- 网络页如实显示本次采集使用的 `wlan0` 地址；采图期间没有触发 NMT、SDO、
  拨号、断开、服务启停或重启操作。

## 触摸状态

Goodix GT911 已在 I²C `0x14` 识别为 ID 967、版本 1060，并注册
`Goodix Capacitive TouchScreen` 与 `/dev/input/event1`。驱动枚举通过，物理触摸已由
现场用户在当前界面实际操作确认可用。

## 边界

这些图片是 framebuffer 内容，不是包含板卡、屏幕边框和接线的相机照片。仓库当前状态
也不代表完成 24 小时老化、外部 CANopen/Modbus/EtherCAT 从站互操作或 IEC 62443
生产安全认证。
