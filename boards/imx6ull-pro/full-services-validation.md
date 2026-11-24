# i.MX6ULL Pro 服务对齐与实板验证记录

日期：2026-11-24。对象：100ASK i.MX6ULL Pro、512 MiB、1024×600，用户明确要求没有外部从站时先做模拟通信。使用同一已授权实板与独立 `/data` 分区，原网络烧录回滚备份保留。

操作与部署教程见[总线、媒体、云接入与应用升级](../../docs/guide/imx6ull-services.md)。本报告记录实测边界，不作为量产验收报告。

## 交付身份

| 项目 | 实际值 |
| --- | --- |
| 应用版本 | `0.1.0-imx6ull.20261124.2` |
| 应用源码提交 | `75cd98597485065521ac46b1890bc6de898eb81f` |
| 应用包 SHA256 | `e2523622d6b86844216ceaf6d6718605fb06afcd87183fdecf195803faa48f77` |
| 压缩包 / 展开大小 | 10981197 / 14511123 字节 |
| 当前应用目录 | `/data/dshancontrol/application/releases/1-e2523622d6b86844` |
| 构建主机工作根 | `/home/ubuntu/dshancontrol-imx6ull-20261124` |
| 架构与运行时 | ARMv7-A hard-float、glibc 2.41、Buildroot 2026.02.1 |
| 实板证据目录 | `/var/lib/dshancontrol-maintenance/services-20261124` |

应用包来自干净提交；本次后续报告、模拟脚本和测试依赖的提交与安装二进制身份分开记录。仅对这一个实验室单元建立升级合同，没有据此确认所有 PCB 版本兼容。

## 接口与软件验证

- 完整 `make test`：345 项，结果 OK，11 项跳过；`make test-hmi` 通过。
- root 身份补充测试最初因非 root fixture 不能遍历主机 home 目录出现 1 项错误。将测试程序复制到任务自己的可遍历临时目录后复测通过，没有放宽原 home 目录权限。要求真实 S4 system daemon 的用例保持跳过。
- 定向检查：可配置 `local_api_port`、5 项 fieldbus、3 项原生 peer 权限检查通过。
- 涂鸦网关 4 项模拟 API 测试通过；没有使用真实项目凭据或真实家庭设备。
- 软件视频 worker 的主机测试通过；ARM 应用及系统依赖已交叉构建并部署。

## 实板结果

| 检查 | 结果与证据 |
| --- | --- |
| 服务身份和健康 | UID 400/401/402、进程实际 executable、HMI commit 心跳、HTTP 与 desktop RPC 通过；`board-service-validation.json` |
| 背光 | 64/128/255 对应驱动 2/4/8，恢复原值 8；不是仅保存设置 |
| CAN 控制器回环 | 收到 `1122334455667788`；测试后 down |
| CANopen 模拟从站 | SDO 读取 `0x2000` 返回 `78563412`，写入 42 收到应答；使用真实 CAN 控制器 loopback |
| 实际 RS485 端口 | `ttymxc2` 可配置，无从站读取明确返回 `NoResponseError` |
| 板端 Modbus 软件模拟 | PTY RTU 读写、CRC 拒绝、超时、参数边界等 5 项通过；`modbus-pty-validation.json` |
| 实际 EtherCAT 接口 | SOEM 扫描无从站返回 `No EtherCAT slaves found`，恢复接口 down |
| 板端 EtherCAT 数据报 | `lo`、EtherType 0x88a4、5 次 BRD/AL-status 响应、WKC=1；`ethercat-datagram-validation.json` |
| 音频传输 | WM8960 左右 PCM 路由修复后，FFmpeg/ALSA 播放和采集传输通过；`media-validation.json` |
| 软件视频 | UID 401、512×288 RGB24，共享帧 442368 字节；完整 5 秒片段结束及暂停/seek/恢复/静音/关闭通过 |
| 正常重启 | runtime、Web、desktop、HMI、音频路由自动恢复，Wi-Fi 恢复连接；`reboot-validation.json` |
| 数据保留 | 系统合同、用户 JSON 配置及随机数据标记哈希保持，SQLite integrity_check=ok；最终数据分区可用约 60.9 MiB |

音频最初因默认 PCM 输出路由关闭而写入失败，已修复并完整复测，最终 `media-validation.json` 取代失败阶段的结果。麦克风采集仅验证 PCM 设备打开和数据传输，未确认拾音质量、未保留录音。软件视频不代表硬件解码、长片稳定性或屏幕目视验收。

## 应用升级和回退

正常升级使用 HMI 的 UID 401，经现有原生任务接口执行：

| 阶段 | 任务 ID | 结果 |
| --- | --- | --- |
| check | `b987be6902f24b3b9806fc858d726a3a` | 识别签名开发版本 `.2` |
| download | `e16ad8cc71c84d32bdfc1d20b64a0030` | 包大小与 SHA256 校验通过 |
| install | `cdb37e07fb4c46438ae031c6d831f8cd` | 健康观察通过，`.1` → `.2`，约 57.8 秒 |
| 故障 install | `e2dd64a56a884f8b980c7138a3db55e2` | 新 HMI 故意退出，健康失败后自动回退 `.2` |

坏签名在检查阶段返回 `signature-rejected`，当前应用保持运行。故障包明确标注开发注入，HMI 为 `exit 73`，不作为发布物。回退后当前身份恢复、健康通过、失败版本进入隔离列表、事务文件移除、正常开发源元数据恢复。

回退脚本最初错误地断言防重放序号应回到 1。核对实现后确认 `high_water=2` 应保持；原脚本在该断言前已验证身份和用户数据标记。修正验证预期并单独复核当前身份、任务错误、隔离列表和健康后通过，未修改 updater 以迎合测试，也未重置序号。下一次更新需 sequence > 2。

升级后与故障回退后分别保存 `ota-install-validation.json`、`ota-rollback-validation.json`。随后执行正常 reboot，检查当前版本、配置哈希、用户标记、数据库和音频路由再次通过。没有开展随机断电测试。

## 云、家庭与语音的剩余输入

板端重启快照：`cloud.configured=false`、`home.configured=false`、`voice.configured=false`、`voice.ready=false`。以下未被软件模拟替代：

1. 百问网设备登记身份、secret、真实绑定接口合同、TLS MQTT 服务及账号归属。
2. 涂鸦项目 Access ID/Secret、区域、授权设备与功能 code；网关已实现，真实云调用未验收。
3. ARM32 hard-float TuyaOpen 音频运行库、audio_subsys/MNN 等依赖与独立 UUID/AuthKey；参考 A133/aarch64 工程不能直接部署。

没有虚构官网设备或家庭设备，也没有以 ALSA 测试替代语音激活、唤醒和对话。

## 交付与维护边界

本轮部署方式为已有系统上的受控文件更新与应用 OTA，没有重做全量 boot/rootfs 镜像。`full-services/ota-75cd985/` 是正常签名开发应用包；故障包目录、板端私有配置、数据库、私钥和系统备份不应发布。

原 eMMC 分区、用户数据和回滚备份保留。为腾出应用升级空间，仅将本任务两个已校验的旧下载镜像副本迁至 rootfs 维护目录，复制前后核对 SHA256，未删除原系统回滚备份。

外部从站、现场维护互锁、真实云账号、涂鸦语音、完整固件再发布、断电恢复及长期稳定性均保持独立验收项。
