# i.MX6ULL Pro：总线、媒体、云接入与应用升级

本章面向 100ASK i.MX6ULL Pro、1024×600 屏幕的开发环境。2026-11-24 已在实板运行 DshanControl，并完成控制器回环、软件模拟通信、音视频数据传输、应用升级及失败回退。没有连接现场从站；官网账号、涂鸦项目授权与 ARM32 语音 SDK 尚未提供，相关业务保持“未配置”，不能把接口测试写成云端联通。

## 1. 先确认交付范围

| 能力 | 当前实现与验证 | 仍需实际环境验证 |
| --- | --- | --- |
| CAN / CANopen | SocketCAN、经典 CAN、NMT、SDO；实板控制器 loopback 收发、模拟 SDO 从站读写通过 | 外部收发器、线缆、终端电阻、真实对象字典；不支持本板 CAN-FD |
| RS485 / Modbus | `ttymxc2` 配置；FC3/FC4 读取、FC6 写入；板端 PTY 真实 RTU 帧、CRC 拒绝、无响应超时通过 | 外部 RS485 电气回环、从站寄存器；保存 `poll_ms` 不会自动启动轮询服务 |
| EtherCAT | SOEM 2.0.0 CLI、扫描、SDO、有限周期诊断；真实网卡无从站错误正确；loopback 数据报模拟通过 | 完整从站发现、CoE、PDO 映射、DC、实时性和实际驱动器 |
| 背光 | 桌面 API → root runtime → sysfs，三级设置与读回通过 | 用户对亮度变化的目视验收 |
| 音视频 | WM8960 输出路由、ALSA 传输、FFmpeg 软件视频、共享帧绘制；完整片段与控制操作通过 | 扬声器听感、麦克风拾音、长片稳定性；没有硬件视频解码资格 |
| 官网绑定与上报 | 现有 HTTPS 绑定、TLS MQTT、桌面快照与有效点位校验已接入 | 登记身份、真实服务参数、绑定响应合同、broker 确认及服务端入库 |
| 涂鸦家庭 | 新增服务端签名网关，设备/指令白名单、能力校验和持久幂等；模拟 API 测试通过 | 项目 Access ID/Secret、区域、授权设备及真实设备回读 |
| 涂鸦语音 | 桌面 Unix socket 接口保留，ALSA 基础传输通过 | ARM32 hard-float TuyaOpen 音频依赖、设备 UUID/AuthKey、激活、唤醒和实际对话 |
| 应用 OTA | 桌面同一 `check → download → install` 任务链；签名、哈希、健康检查、回退和数据保留通过 | 正式发布源、公钥管理、全型号资格、断电恢复试验 |

实板当前版本为 `0.1.0-imx6ull.20261124.2`，应用源码提交 `75cd98597485065521ac46b1890bc6de898eb81f`。后续文档和测试依赖提交不改变这个已安装二进制身份。

## 2. 理解接口和进程边界

| 入口 | 服务身份 | 用途 |
| --- | --- | --- |
| `127.0.0.1:8080/api/hmi/v1` | Web UID 400 | 本地查询、设置及总线动作封装 |
| `/run/dshancontrol/platform.sock` | root runtime | 受限硬件动作、背光、应用升级任务 |
| `/run/dshancontrol/desktop/desktop.sock` | desktop UID 402 | 官网、上报、家庭、语音状态与命令 |
| Qt HMI | UID 401 | 界面、触摸、媒体播放与共享帧绘制 |

总线请求由 Web 通过 Unix socket 转发给 runtime。runtime 检查对端 UID、方法权限、板级通道、参数以及维护互锁，再运行固定的 Python 执行器；请求不能指定任意程序或 shell。Web 可调用 `fieldbus.execute`，HMI 不直接获得该方法权限。

硬件控制和 OTA 共用控制锁；系统、应用与用户数据分别管理。实验室维护互锁验证目标板身份、eMMC 布局、独立 `/data` 挂载，以及 CAN、RS485、EtherCAT 未接入现场工作的状态。接入实际设备后必须换成对应设备的安全停机流程。

HTTP 完整合同见[后端接口](../backend-api.md)。桌面服务配置必须包含 `local_api_port: 8080`，避免桌面采样仍访问旧板的 80 端口。

## 3. 编译与测试

构建主机使用 Ubuntu，BSP、工具链与完整系统准备步骤见[板级编译教程](../../boards/imx6ull-pro/README.md)。本轮交叉构建使用 Buildroot 2026.02.1、ARMv7-A hard-float、glibc 2.41。

在源码根目录运行：

```sh
make test-bootstrap
make test
make test-hmi
```

`tests/requirements.txt` 固定了 Modbus 模拟测试所需的 `minimalmodbus==2.1.1` 和 `pyserial==3.5`。最新完整 Python 回归为 350 项、12 项跳过，Qt/HMI 回归通过；需要 root/真实系统服务的检查单独记录，不把跳过计入实板通过数量。

应用构建快照必须来自已提交源码；修改主 checkout 不会自动更新旧 external tree。依赖包括 SOEM、python-can、canopen、minimalmodbus、ALSA 和 FFmpeg。只执行包构建不会更新 `images/` 下的完整固件。本轮在已有实板系统上部署依赖并完成应用 OTA，没有重新生成全量启动/rootfs 镜像，旧镜像不可当作本次最新交付。

## 4. 无从站时验证总线

### CAN 控制器回环与 CANopen

平台映射使用 `can0`。本轮实板以控制器 loopback 验证 8 字节 `1122334455667788` 的收发，再通过软件从站响应 SDO 请求，读取 `0x2000` 和写入数值 42 均得到协议应答。测试后恢复 CAN 为 down。

动作示例用于**已确认没有现场设备的实验室板**，由维护程序经现有 HMI HTTP 动作入口提交：

```json
{"action":"can_apply","params":{"interface":"can0","bitrate":500000,"loopback":true},"confirmed":true}
```

`can_send` 和 `canopen_nmt` 返回 `queued:true`，不代表外部从站确认。SDO 只有收到有效响应才返回 `device_acknowledged:true`。没有 EDS/PDO 映射时，不使用历史 RPDO 表单按变量名称写入；当前 i.MX6ULL 原生执行层不提供 RPDO 操作。

### Modbus RTU

实际串口映射为 `/dev/ttymxc2`，与控制台串口分开。配置动作接受 `port`、`baudrate`、`unit_id`、`poll_ms`，可选 `parity` 与 `stopbits`；配置原子写入 root 私有状态目录。

```json
{"action":"rs485_config","params":{"port":"/dev/ttymxc2","baudrate":9600,"unit_id":1,"poll_ms":1000,"parity":"N","stopbits":1},"confirmed":true}
```

读取动作 `rs485_read` 使用 `address`、`count`、`function`；写入动作 `rs485_write` 使用 `address`、`value`，默认 FC6。地址为协议零基地址，不直接把手册中的 `40001` 当作 PDU 地址。只有收到并校验响应后才确认设备应答。

在 Linux 主机运行可重复的软件测试：

```sh
.test-venv/bin/python -m unittest discover -s tests -p test_platform_fieldbus.py -v
```

测试在 PTY 上发送真实 RTU 帧，验证 FC3/FC6、CRC 和超时。相同测试已在板上使用已安装的执行器跑通。PTY 测试不经过 RS485 收发器；真实串口未连接从站时返回 `NoResponseError` 是预期结果。

### EtherCAT

平台映射使用 `eth1`。SOEM 扫描真实接口未发现从站时返回失败，不伪造设备列表。有限周期诊断的 `count × period_us` 最多 5 秒，执行器有独立超时限制。

软件数据报实验固定使用 Linux `lo`，不会选择物理网卡：

```sh
sudo python3 scripts/ethercat_datagram_lab.py
```

脚本使用真实 `AF_PACKET` 和 EtherType `0x88a4`，交换 5 次 BRD/AL-status 请求与模拟应答，检查 `WKC=1`。板端测得约 2.9–4.2 ms 往返仅为本次模拟观测，不能解释为 EtherCAT 周期性能。此实验不实现完整从站状态机、CoE、PDO 或分布式时钟。

## 5. 背光和音视频

背光动作接受 10–255 的逻辑值，由原生驱动按设备 `max_brightness` 映射。实板请求 64/128/255 时，驱动读回 2/4/8；测试结束恢复原值 8。保存显示设置与实际写背光是两步，验收应检查动作响应和 sysfs 读回。

`S97dshan-audio` 初始化 WM8960 左右 PCM 输出混音通道，默认 Speaker 80%、Headphone 70%。此板初始路由关闭会导致 PCM 写入失败；已修复并在重启流程中启用。初始化脚本不启动录音。

媒体 worker 使用 `CONFIG+=software`：FFmpeg 解码本地普通文件，拒绝网络 URL 和软链接，输入尺寸上限 1280×720，输出 512×288 RGB24、10 fps。帧经 Qt 共享内存交给 HMI 绘制，不直接抢占 `/dev/fb0`。

实板 UID 401 下完成测试片全部播放及暂停、seek、恢复、静音、关闭；共享帧长度为 442368 字节。ALSA 播放、采集设备的数据传输均通过，但未凭软件返回值确认扬声器听感或麦克风拾音，未保留录音。

## 6. 官网绑定和云端上报

官网与家庭服务使用独立凭据。板上 `/etc/dshancontrol/desktop.json` 由 UID 402 持有，模式 0600；状态目录为 `/data/dshancontrol/provider-imx6ull`，模式 0700。合并业务配置时保留 `local_api_port: 8080` 和已有状态目录，不覆盖用户布局或数据库。

官网需由设备登记系统提供 `cloud.device_id`、`cloud.pairing_url`、`cloud.secret_file` 及 TLS MQTT 参数，字段结构见[配置参考](configuration.md)。公开官网页面地址不能代替设备登记 API，也不能自行生成一个设备编号假装已登记。

验收顺序：设备身份匹配 → HTTPS 查询绑定码与有效期 → 官网账号绑定 → 再查询归属 → MQTT TLS 连接 → 有效点位上报 → broker 确认 → 服务端查询入库。`accepted`、MQTT PUBACK 和业务入库是不同阶段，界面分别保留状态。

目前缺真实设备登记凭据和服务端联调信息；软件保持未配置状态。完整操作见[官网绑定与上报](cloud.md)。

## 7. 部署涂鸦家庭网关

`components/integrations/tuya_gateway.py` 将涂鸦 Cloud API 转换为桌面已有的 `/v1/devices`、`/v1/devices/{id}/functions` 和 `/commands` 接口。项目 Access Secret 留在网关主机，板上只放受限访问令牌。

网关配置结构如下，所有值均为占位符，需由项目管理员提供实际内容：

```json
{
  "region": "cn",
  "access_id": "PROJECT_ACCESS_ID",
  "access_secret_file": "/srv/dshan-tuya/private/access-secret",
  "desktop_token_file": "/srv/dshan-tuya/private/desktop-token",
  "command_database": "/srv/dshan-tuya/private/commands.sqlite",
  "devices": {"AUTHORIZED_DEVICE_ID": ["switch_1"]}
}
```

配置和凭据必须为服务用户拥有的 0600 普通文件，私有目录为 0700。令牌需使用随机高熵值，长度 32–1024，不能使用示例文本。区域必须匹配涂鸦项目数据中心。设备 ID 与功能 code 均显式授权，禁止默认开放项目下所有设备。

网关使用 Flask WSGI 工厂。Linux 主机在受控虚拟环境安装 Flask 与生产 WSGI server 后，从仓库根目录启动，例如：

```sh
export DSHANCONTROL_TUYA_GATEWAY_CONFIG=/srv/dshan-tuya/private/gateway.json
gunicorn --bind 127.0.0.1:18081 --workers 1 --threads 4 \
  'components.integrations.tuya_gateway:configured_app()'
```

外层 HTTPS 反向代理提供有效证书；桌面 `home.endpoint` 指向该 HTTPS 地址，`home.token_file` 指向 UID 402 可读的私有令牌文件。不要把上述 loopback HTTP 端口直接配置为桌面公网接口。

网关按涂鸦官方 HMAC-SHA256 合同签名并刷新 token。桌面提交单条 `{"code":"switch_1","value":true}`，网关转换为涂鸦上游的 `{"commands":[{"code":"switch_1","value":true}]}`。功能类型统一为桌面识别的 `Boolean/Integer/Enum`，每台设备最多授权 24 个功能。控制请求先校验设备在线状态、功能类型/范围与白名单，提交前持久预留幂等键；超时或崩溃后不自动重发。`accepted:true,state_confirmed:false` 表示云端受理，最终设备状态需再次同步。

当前 7 项网关测试通过，其中一项运行真实 Qt 桌面进程，经 HTTPS 调用实际 WSGI 网关，对接模拟涂鸦 API，验证单次控制、等待确认与状态回读；另有网关重启后幂等键保留检查。通过不代表真实项目已授权，仍需 Access ID/Secret、区域、账号设备授权后完成实测。

## 8. 接入涂鸦语音

桌面 `voice` 配置需要 Unix socket 路径与实际运行时 UID；客户端校验对端身份，并分别显示配置、激活、录放音和心跳状态。

现有参考工程面向 A133/aarch64。i.MX6ULL 是 ARM32 hard-float，不能直接使用该二进制。仍需匹配架构和 libc 的 TuyaOpen 音频 SDK、`audio_subsys`、MNN 等依赖以及独立设备 UUID/AuthKey，然后实现真实语音 runtime 与桌面 IPC 的连接。ALSA 可用仅证明底层音频传输，不能替代唤醒、识别、合成和对话验收。

## 9. 使用桌面应用 OTA

系统 runtime 保持在 `/usr/bin`，应用按版本安装到 `/data/dshancontrol/application/releases/`，`current` 为原子切换的版本链接。用户配置、布局和数据库位于独立 `/data` 目录，应用包不携带这些数据。

开发环境没有正式发布地址，因此本轮采用显式 `development_mode`、关闭自动安装的本地签名发布源。源目录为 root 私有目录；生产 HTTPS/GitHub 发布规则没有放宽。板上仅部署公钥，签名私钥留在构建主机私有目录，不进入源码、应用包或证据压缩包。

通过 `update.start` 创建检查、下载、安装任务，再使用 `update.status` 查询。安装确认绑定下载任务、版本和包 SHA256，不能用旧页面的确认去安装另一份包。完整合同见[应用更新教程](updates.md)。

本轮验证：

1. UID 401 发起检查、下载、安装，从 `.1` 升至 `.2`；新版本健康观察通过。
2. 损坏签名在检查阶段被拒绝，当前应用未切换。
3. 使用明确标注的开发故障包令 HMI 退出，健康检查失败后自动切回 `.2`，恢复健康并隔离失败版本。
4. 配置、用户数据标记与 SQLite 完整性通过；普通重启后继续校验服务和数据。

失败包使用 sequence 2；回退后 `high_water` 保留 2，属于防重放设计，不应重置。下一份合法开发升级必须使用更大的序号。故障包只用于本轮测试，不是发布物。

当前正常包 SHA256：

```text
e2523622d6b86844216ceaf6d6718605fb06afcd87183fdecf195803faa48f77
```

本轮升级资格只覆盖该实测实验室单元，PCB 版本尚未独立核验。应用 OTA 不改变 eMMC 分区，也不等价于系统 A/B 升级或完整固件烧录。

## 10. 复核证据与后续验收

详细板端记录见[本轮验证报告](../../boards/imx6ull-pro/full-services-validation.md)。后续连接真实设备时，先提供端口、接线、波特率、站号、对象字典/寄存器表及维护要求，再按相同接口验证实物响应。云端与语音则需私有凭据和匹配架构的 SDK；这些输入齐备前，相关功能保持明确的未配置状态。
