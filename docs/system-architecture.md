# DshanControl 系统架构

![DshanControl 工业控制桌面系统架构](images/system-architecture.svg)

架构遵循单向依赖：应用只访问稳定 API/MQTT 契约，平台服务调用协议适配器，协议适配器
再通过硬件 Manifest 解析板级资源。这样更换 SoC 或 Linux 设备名时，不需要修改业务应用。

```mermaid
flowchart TB
  subgraph APP[应用与交互层]
    HMI[Qt 本地 HMI]
    WEB[Web 管理桌面]
    NR[Node-RED / 第三方应用]
  end
  subgraph CONTRACT[稳定接口与数据契约]
    API["/api/v1 与 /api/hmi/v1"]
    MQTT[MQTT 遥测与受控命令]
    SCHEMA[JSON Schema]
  end
  subgraph PLATFORM[平台服务层]
    AUTH[认证 / RBAC / CSRF / 审计]
    MODEL[设备与点位模型]
    STORE[SQLite / 原子配置]
    HEALTH[健康检查 / supervisor]
  end
  subgraph ADAPTER[协议与通信适配层]
    CAN[CAN / CAN-FD / CANopen]
    MODBUS[RS485 / Modbus RTU]
    ECAT[EtherCAT / SOEM / CoE]
    NET[Ethernet / Wi-Fi / 4G / MQTT]
  end
  MANIFEST[硬件抽象：/etc/omnigate/platform.json]
  BOARD[Linux / Buildroot / Tina / DTS / 驱动 / 板级分区]
  HW[T153MX / T113s4 SD NAND 开发接入 / 后续独立适配]

  APP --> CONTRACT --> PLATFORM --> ADAPTER --> MANIFEST --> BOARD --> HW
```

## Profile 边界

- Lite：原生 Core、Web、Qt HMI、supervisor、SQLite 和协议适配器，不安装 Node-RED。
- Enhanced：复用相同契约，增加受限 OCI 应用和 Node-RED；容器没有 CAN、串口、GPIO
  或 EtherCAT 原始网卡权限。

上方 SVG 保留历史 T153 架构概览；本文 Mermaid 与
[发布边界](../platform/docs/architecture.md) 描述当前扩展。S4 使用独立 SD NAND 分区
和 ext4 UDISK，不沿用 T153 A/B 分区。两块板须分别验收；T153 短测不覆盖当前 UI 或 S4。
A133/T527/RK3576 为未来目标，RK3568/T536 Enhanced 也仅有主机配置验证。
架构与配置不表示外部从站互操作、24 小时老化或生产安全认证已经通过。
