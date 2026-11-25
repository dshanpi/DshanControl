# 验收 i.MX6ULL Pro 原生服务

本报告记录 2026-11-25 在 100ASK i.MX6ULL Pro、512 MiB 内存、1024×600 LCD 上的验证。软件开发与编译使用 Ubuntu 主机；板端通过串口维护和局域网传输更新。外部 CAN、RS485、EtherCAT 从站未连接。

## 核对安装身份

| 项目 | 已安装内容 |
|---|---|
| 应用版本 | `0.1.0-imx6ull.20261124.3` |
| 应用源码 | `f549f80ca9bf0b8c6a5552b2252073d57b85f136` |
| 正常应用目录 | `/data/dshancontrol/application/releases/3-5d5874c570e35a24` |
| 应用包 SHA256 | `5d5874c570e35a24fb7094beffdcb2b1bfb80bd512f524491dc6e40695e77457` |
| 板端 API / 总线 / 生命周期 | C++，QtCore/QtNetwork |
| 桌面 / 媒体 | C++ / Qt，FFmpeg 软件解码 |
| 涂鸦网关 | Go，HTTPS，SQLite 持久幂等 |
| 系统生命周期修复源码 | `8170287`，包含当前进程心跳检查与开机背光恢复 |
| 生命周期二进制 SHA256 | `1e54b721384a8f9f5fc6d049a3dea3a7ea53f1b6179c6c42e6f053c36e99947b` |

系统组件、应用和用户数据分别保存。迁移前保存系统文件和停止后的数据库备份；未覆盖挂载中的 rootfs，未改变 eMMC 分区。系统使用 ARMv7-A hard-float、glibc 2.41。应用来自干净 Git 提交；系统生命周期修复是独立构建和部署的系统组件，不改变应用提交身份。

在板端执行以下命令复核：

```sh
readlink -f /data/dshancontrol/application/current
cat /data/dshancontrol/application/current/installed.json
/usr/libexec/dshan-imx6ull-services maintenance
/usr/libexec/dshan-imx6ull-services health
wget -q -O - http://127.0.0.1:8080/api/health
```

预期健康检查 `ok=true`，API 返回 `backend=native-cpp`，应用提交与上表一致。PID 身份为 API 400、HMI 401、desktop 402；root runtime 只提供受限平台操作。

## 验证触摸和显示

实板 Goodix GT911 驱动已识别 1024×600 坐标范围，但 udev 默认将 `goodix-ts` 分类为 tablet，Qt 因此没有打开触摸节点。系统规则按设备名称修正分类并创建稳定链接，不依赖 event 枚举顺序。

修复后 `ID_INPUT_TOUCHSCREEN=1`，HMI 已打开实际 event0；用户确认“点击和位置都正常”。event1 是 GPIO 按键，不能当作触摸设备。此项包含真实用户触摸确认，未用注入点击替代。

背光经桌面动作 API、root runtime、sysfs 执行。请求 64/128/255，驱动读回 2/4/8，测试结束恢复原亮度。

随后发现设置文件已保存、驱动在重启时仍回到默认亮度。现由 `S98dshan-runtime` 在平台服务启动后调用原生 `display-restore`，通过同一受限 RPC 应用保存值，避免在 OTA 持锁的生命周期钩子中重复请求控制锁。设置 128 对应驱动值 4。

## 核对通信与媒体结果

| 验证项 | 结果与限制 |
|---|---|
| CAN 控制器回环 | 8 字节收发通过，测试后恢复 down；没有外部从站 |
| CANopen | 4 项专项通过：快速和分段 SDO、toggle/对象/abort/超时拒绝、扫描/NMT、PDO 映射和变化拒绝 |
| Modbus 原生执行器 | 4 项板端 PTY 测试通过：FC3/FC6、CRC 拒绝、超时、未授权端口拒绝 |
| 实际 UART | `/dev/ttymxc2` 配置通过，无从站时返回 `No Modbus response` |
| 实际 EtherCAT 网卡 | eth1 无 carrier，SOEM 扫描正确返回无从站，测试后恢复 down |
| EtherCAT 数据报模拟 | lo 上 5 次 BRD/AL-status、WKC=1，通过；不等于 SOEM 从站发现、CoE/PDO/DC 资格 |
| ALSA | WM8960 设备、采集打开和传输、播放传输通过；未保留录音，听感需人工验收 |
| 软件视频 | UID 401 完整播放、暂停、seek、恢复、静音、关闭通过，输出共享帧 512×288；无硬件解码声明 |
| Go 网关 | 7 项含 race 检查通过；真实 Qt → HTTPS → Go → 模拟涂鸦链路验证，真实账号未配置 |

FlexCAN 硬件 loopback 同时产生控制器 RX 和本地 TX echo。板端软件从站测试通过 `recvmsg` 标识排除本地 echo，并关闭从站响应的软件 echo，避免将同一帧处理两次；原生生产执行器没有为测试放宽 SDO 校验。PTY、模拟 CANopen 和 EtherCAT 数据报测试由一次性测试驱动执行，测试驱动不属于常驻服务。

## 检查升级与恢复

正常 `.2 → .3` 更新通过 UID 401 的签名 OTA 任务链完成。安装后用户配置和数据标记保留，SQLite `integrity_check` 为 `ok`，原生应用包不含 Python 文件。

故障测试使用未修改的合法应用包，只在新版本启动后精准终止该目录中的 API。sequence 4 首次暴露同提交旧心跳误判：已切回旧版本，但事务保留。随后通过原生 `recover` 完成恢复，未手工删除事务或降低防重放计数。修复后的生命周期必须识别当前 GUI PID 和 boot ID，并使用单调时钟判断新鲜度。冻结 GUI 并将心跳 PID 改为旧进程值的负向检查已拒绝，恢复真实心跳后健康通过。

修复后的 sequence 6 故障升级自动回退通过：仍运行正常 sequence 3，`high_water=6`，失败版本被隔离，事务文件已清理。坏签名在检查阶段返回 `signature-rejected`。以上任务由原生 RPC 测试客户端执行，板端 Python 入口均不可执行，未使用 Python 业务进程。

随后保持解释器不可执行并普通重启，Wi-Fi 自动重连、原生健康检查、Goodix 分类和 HMI 打开触摸节点均通过。验证后恢复旧解释器文件以保留旧版恢复能力，新应用没有切回 Python。SQLite 两个数据库完整性为 `ok`；布局、系统合同和数据标记哈希一致。用户在触摸验收期间通过界面调节亮度，审计记录与保存值 128 一致，该操作被保留，未伪称此设置文件哈希未变化。

保留 sequence 4 的失败测试和显式恢复记录，不把首次失败改写为通过。测试下载缓存及失败版本仅归档到 rootfs 私有维护目录，原数据分区与用户数据库未清除。

## 复核主机回归与时间设置

Ubuntu 最新 `make test` 通过：core 5 项、整仓 372 项，其中 12 项按环境条件跳过；跳过项不计为实板通过。原生 API 专项 9 项、Go 网关 7 项（含 race 与真实 Qt HTTPS 链路）通过。`make test-hmi` 退出码 0；[原生迁移提交 CI](https://github.com/dshanpi/DshanControl/actions/runs/45429478331) 的 make test、make test-hmi 和仓库检查均通过。

本板初始 RTC 为 1970 年，校准到维护主机提供的 UTC 时间并写入 RTC；安装 Asia/Shanghai 时区后再次普通重启，时钟、布局、显示设置与时区文件保留。此结果不覆盖掉电后的 RTC 电池保持能力。本镜像未配置 NTP 守护进程，正式云接入前需配置可信校时源；不要通过关闭 TLS 证书校验绕过时间错误。

实板证据见仓库 `docs/validation/imx6ull-native-20261125/`，包含总线、媒体、升级、坏签名、自动回退和无 Python 重启结果。原始私有备份及签名私钥未发布。

## 确认实际业务边界

官网绑定、TLS MQTT 上报、家庭设备和语音 IPC 已保持桌面接口合同；本板未提供设备登记凭据、涂鸦项目授权、真实家庭设备和 ARM32 语音运行时，因此保持未配置。不能将接口模拟通过解释为真实官网绑定、涂鸦设备控制或语音对话成功。

无正式 OTA 发布源和生产签名公钥；本次使用显式开发模式的私有本地签名源。没有资格声明涵盖系统 A/B、断电升级、外部总线电气连接、EtherCAT 实时性能、长时间稳定性或硬件视频解码。Python 构建和测试脚本仍可保留在源码；本次迁移目标是取消板端生产服务对解释器的依赖。

按[操作与验收教程](../../docs/guide/imx6ull-services.md)完成屏幕操作和后续真实设备验收。
