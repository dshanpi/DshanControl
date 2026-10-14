# Architecture

DshanControl 把工业控制桌面拆成四层，板级差异只能向下收敛。
现有 `omnigate` 服务名、API 和路径是 T153 兼容接口，不是另一个产品。

1. **应用层**：Qt HMI、Web、Node-RED 和后续第三方应用，使用版本化 API
   （包括 `/api/v1`、本地 HMI 的 `/api/hmi/v1`）或 MQTT。
2. **平台服务层**：认证、设备/点位模型、SQLite、审计、健康检查和 supervisor。
3. **协议适配层**：CANopen、Modbus RTU、EtherCAT、蜂窝和网络适配器，把厂商数据
   转换成统一设备与点位模型。
4. **板级支持层**：内核、DTS、启动、设备节点、显示和厂商固件，通过
   `/etc/omnigate/platform.json` 暴露逻辑能力。

依赖方向固定为“应用 → 平台 → 协议 → 板级”。应用层禁止导入板级脚本，容器禁止
获得硬件设备权限。Lite 使用本机进程；Enhanced 可增加 OCI 应用，但二者共享相同 API、
数据模型和硬件 Manifest。

T153 持久数据默认位于 `/var/lib/omnigate`。S4 由系统契约固定在启动映射的可写 ext4
UDISK：应用版本为 `/mnt/UDISK/dshancontrol/apps`，数据为 `/mnt/UDISK/dshancontrol/data`。
数据在版本目录外；挂载失败不能触发格式化。配置采用临时文件加原子替换，设备和审计数据
使用 SQLite。所有服务必须提供健康状态，supervisor 只负责有限次数恢复，不代替启动
系统、硬件看门狗或 A/B 回滚。

## 应用与系统发布边界

分层不等于发布包边界。应用包包含桌面、Web、核心/业务服务和资源；BSP、内核、DTS、
Bootloader、分区、基础库、系统身份、信任根、更新器及维护/健康钩子均归系统发布。
兼容应用更新不要求新硬件提交、SDK 重建或系统镜像，也不改写工厂预装身份。
安装须验证完整系统契约、签名、载荷哈希和维护状态；GUI 安装仍禁用。
具体以 [发布门禁](../../docs/RELEASING.md) 与
[应用更新协议](../contracts/application-update-v1.json) 为准。

EinkOS 仅为发布分层参考，SmartDesktopMimi 是独立未来项目。T153 历史短测不覆盖
S4 或新增更新器；A133/T527/RK3576 仅为未来适配方向，不能由其他板的通过推断支持。
