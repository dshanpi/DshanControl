# DshanControl 原生更新运行端（C++）

量产框架语言选型：C++11 + Qt Core/Network + OpenSSL + zlib，复用现有 HMI/SDK
工具链。原生更新核心、验签、解包、事务恢复和任务服务均由 C++ 执行。
已有 Python 更新器保留为迁移参考和协议回归入口；系统集成尚未默认切换。
旧 Web 业务及现有系统健康/初始化钩子的 Python 实现仍需按后续阶段迁移。

当前包含：严格 JSON/重复键拒绝、RSA/SHA-256 验签、系统兼容匹配、GitHub
兼容发布选择、流式有界下载、安全 tar/PAX 解包、资源锁、fsync/原子状态、
防重放、事务安装、启动观察、已知良好回退、CLI 与独立 Unix socket 任务服务。
HMI 已接通原生任务协议下的下载、安装确认和阶段/错误显示；亮度控制已加入原生
平台接口并提供旧 Web 的显式迁移入口；其余硬件平台操作迁移、
原生自动检查/安装策略与正式系统集成仍待完成，不标成完整平台服务或生产 OTA 就绪。

## 构建与验证

```sh
make build-native
make test-native
make test
```

`make test` 先构建原生主机程序，再运行全部回归。测试脚本和合成测试钩子可以
使用 Python；实际被测安装器、任务服务是 C++ 可执行文件。

S4 交叉编译示例（在仓库根目录执行）：

```sh
mkdir -p dist/native-arm
/home/ubuntu/T113-tina5v1.2-sdk/out/dshancontrol-s4-dev3/buildroot/host/bin/qmake \
  -o dist/native-arm/Makefile components/runtime/runtime.pro
make -C dist/native-arm -j3
```

dev3 qmake 复用 dev2 的现有 sysroot，不能据此声称是新的已验收系统。
未来主板必须使用各自已固定的 sysroot 和目标构建；当前只有 S4 交叉构建证据。

## 管理员 CLI

```sh
dshancontrol-runtime --config /etc/dshancontrol/updater.json check
dshancontrol-runtime --config /etc/dshancontrol/updater.json download
dshancontrol-runtime --config /etc/dshancontrol/updater.json \
  install application-release.json application-release.json.sig TARGET.tar.gz
dshancontrol-runtime --config /etc/dshancontrol/updater.json recover
```

配置遵循既有 updater.json 和 application-release v1；系统拥有的 service_hook
提供 maintenance/stop/start/health。未配置正式公钥、身份、维护互锁时不绕过门禁。
`control_lock` 可由系统配置，缺省为受管应用根下 `.control.lock`；未来硬件平台服务
必须共用此锁才能阻止维护窗口内的新控制操作。`development_mode` 只允许 JSON
布尔值，生产设备拒绝草稿。整数序号/大小按 JSON 可精确表达的范围校验。
正式模式还必须提供系统契约的 `storage.mount_point`、`storage.device`、
`storage.filesystem=ext4`，并匹配实际挂载的可写设备。检查发生在建立升级目录之前，
未知、只读、错设备或根文件系统回落均拒绝；不会格式化数据卷。

原生 UI 模式由系统 updater.json 中的 JSON 布尔 `task_service=true` 启用；
系统同时需要部署原生服务及将 `/usr/bin/dshancontrol-update` 指向原生运行端。
没有该标记的旧基线继续保持原检查界面，不能仅设置标记就宣称基线可升级。
系统启动器可用 DSHANCONTROL_UPDATER_CONFIG/DSHANCONTROL_UPDATER_EXECUTABLE
选择系统拥有的配置和客户端，缺省路径保持兼容；HMI 的路径选择不改变原生服务的
身份、签名和权限门禁。原生任务的 Unix socket 仍由系统固定，不接受 GUI 的网络目标。

## 独立任务服务

```sh
dshancontrol-runtime serve /etc/dshancontrol/platform-service.json
dshancontrol-runtime rpc update.identity '{}'
dshancontrol-runtime rpc update.start '{"operation":"check"}'
dshancontrol-runtime rpc update.status '{"task_id":"SYSTEM_RETURNED_TASK_ID"}'
```

平台服务配置包含系统固定的 `socket`、`updater_config`、`task_root`、
`client_gid`、`allowed_uids`；使用 SO_PEERCRED 核对 Unix 调用方，不开放网络监听。
配置由系统管理，客户端不能提交设备路径、系统身份、更新 URL 或 Shell 命令。

任务开始即持久化固定身份和任务 ID。下载引用成功的检查任务；安装引用成功的
下载任务并明确确认 version/package_sha256。系统再次核对清单签名、哈希、来源、
兼容条件和维护状态。下载时绑定选中清单及包哈希，不能静默改成后来出现的 Latest。
安装归服务线程所有，提交客户端断开不会取消安装。每阶段记录耗时，错误保留阶段
和原始退出码。重启只检查持久升级事务，并将未结束任务标为 interrupted；
不自动重发下载/安装，故障恢复与已知良好回退继续执行。

准备状态、当前任务和历史任务按 task_id 区分。`update.status` 不带 ID 时返回最新
任务，无历史时返回 null；不能将历史成功解释为刚点击的新操作成功。
未接入取消/自动安装策略前不提供这些操作，缺失的方法明确拒绝。

## 原生亮度接口

系统平台配置可增加以下绑定；缺失时亮度方法拒绝，不猜测其他显示设备：

```json
{
  "platform_manifest": "/etc/omnigate/platform.json",
  "display": {"channel": "display0", "backlight": "backlight0"}
}
```

```sh
dshancontrol-runtime rpc display.brightness.get '{"channel":"display0"}'
dshancontrol-runtime rpc display.brightness.set \
  '{"channel":"display0","value":120,"confirmed":true}'
```

服务核对平台清单的板型、架构及显示通道。客户端不能指定设备路径、别的板卡或
执行命令；生产固定使用 /sys/class/backlight，测试目录覆盖仅允许开发 fixture。
逻辑亮度为整数 10..255，按驱动 max_brightness 换算后读取 brightness 确认。
返回 source=driver-configured-brightness，表示驱动配置读回，不冒充实测屏幕亮度。
写入共用更新器 control_lock；维护占用时拒绝，不等待重放、不自动重试。

S4 厂商系统实测没有标准 backlight 类和 lcdbl 属性，使用已核对的 /dev/disp LCD0
ioctl。该板选择 `display={"channel":"display0","backend":"sunxi-disp2","screen":0}`；
服务固定设备和操作码，确认输出类型为 LCD，再写入/读回 0..255 配置值，客户端
不能指定路径、screen 或 ioctl。此后端仅开放给已审计的 S4 板型/SoC，不推断其他
Allwinner 板也适用；原 sysfs 后端仍保留供各板独立适配。实板测试 100→160→100、
维护占用拒绝以及应用健康/系统身份保留已通过，仍不代替完整 OTA 验收。

旧 Web 仅在系统显式设置 DSHANCONTROL_NATIVE_PLATFORM_SOCKET 时通过 Unix RPC
转发亮度；原生通道失败不回落到直接写 sysfs。未配置的旧 T153/S4 基线保持兼容。
这仍是渐进迁移，尚未代替系统启动集成、硬件操作完整审计和非特权应用隔离验收。

## 本轮证据（2026-10-14）

- 23 项原生测试通过：真实 CLI 进程、锁、维护拒绝、篡改/错配、SIGKILL 前后恢复、
  回退失败保留事务、PAX/包边界、重复 JSON、Unix 调用方及独立任务。
- 亮度主机测试覆盖驱动换算/读回、绑定错配、注入/参数拒绝、维护锁以及失败不重试；
  Web 迁移入口的转发和失败不回落通过。全仓库最新结果见 ITERATION-ACCEPTANCE。
- `make test-hmi`：34 项通过，包含原生下载来源绑定、安装确认、阶段/原码显示及失败不重试。
- S4 ARMv7 EABI5 softfp 交叉构建通过；通过 WiFi 传输并校验试验包。
- S4 隔离 ext4 目录中原生程序完成测试签名包安装、防重放、数据保留检查，
  实际系统契约和出厂应用记录哈希保持一致，退出码 0。
- S4 原生服务的本地 RPC、身份/任务投影、重复实例拒绝、残留 socket 恢复通过。

板端使用合成应用载荷、临时测试签名及 fixture-only 系统契约；私钥不传到设备。
这证明原生程序能在当前目标运行并执行核心协议，不能替代真实 HMI/Web 升级、
GitHub 发布、现场维护、物理掉电及整体 OTA 验收。
证据：`dist/iteration-h05-platform-update/`、
`dist/hardware-mcp/native-board-smoke.json`、`native-service-board-probe.json`。
板端记录对应各自试验归档，早于最新存储/子进程约束及亮度接口改动；不能据此把
最新交叉编译程序标成已完成实板亮度控制或正式应用升级验收。
