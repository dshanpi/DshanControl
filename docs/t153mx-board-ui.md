# T153MX 实板桌面

完整图库位于 README 顶部。以下文件均来自同一块 T153MX 的真实 framebuffer：

| 页面 | 文件 | SHA-256 |
| --- | --- | --- |
| 运行总览 | `01-overview.png` | `65e19d3b6821ed03bd124c3c69674cd810489c99a53a1d27b7c5b210e81981c6` |
| CAN / CANopen | `02a-fieldbus-can.png` | `729fba97d55180a93739d3272568cef8b45a428279237f69046c8dcc92a66ef0` |
| RS485 / Modbus | `02b-fieldbus-modbus.png` | `b7950ea502da43c27a30ccb3327993c918f71f998c24c8ee38ec148269d00` |
| EtherCAT | `02c-fieldbus-ethercat.png` | `acf52bc1f3a6af540b6af37d3eefb2ceff6534e703d1486b3ccd24fb3874d856` |
| 网络通信 | `03-network.png` | `3949a8ebc4b7e6c6e1f543248f77faeeb1b1125ec3f33168dcea45c5e7b03772` |
| 设备监控 | `04-devices.png` | `f9a1dc3ed7b28651a0ff9e4b6566aa46c3098dbbb7fe5e92851c27d6c26d0c16` |
| 告警记录 | `05-alarms.png` | `c3fe31329ff2b89ea5bc251c69d661d294193d6d59a79219dddee467ffd67b37` |
| 系统设置 | `06-settings.png` | `ea652fd9a62b32d3f29be5076f037bd4a4f9931b4a83cfa879488e458b674342` |

## 证据来源

- 采集时间：2026-09-16 11:26–11:27 UTC
- 采集方式：板端 `fbgrab` 直接读取真实 framebuffer
- 传输方式：板卡通过 Wi-Fi `wlan0` 获取地址后，由临时只读 HTTP 服务传输；
  板端与本地逐图 SHA-256 一致
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
