# i.MX6ULL Pro 原生系统镜像交付 · 2026-11-25

本次已上传 [GitHub 开发草稿](https://github.com/dshanpi/DshanControl/releases/tag/untagged-e8d3214c50638ad44acc)，系统标签为 `system-v0.1.0-dev.20261125.1--imx6ull-pro`。
草稿需仓库相应权限查看，不是公众稳定版，也不进入桌面自动更新源。
全部 **16 个实际远端附件**已重新下载，逐项大小/SHA256、清单闭合、压缩镜像解压结果和应用/硬件/BSP 源码身份均核验通过。
详细回执见 [remote-verification.json](validation/imx6ull-system-20261125/remote-verification.json)。

## 应用与主板源码

| 范围 | 固定来源 |
| --- | --- |
| 工厂应用 | [DshanControl `ab981454fd095fb1101bea317ab8dcc33d94897b`](https://github.com/dshanpi/DshanControl/commit/ab981454fd095fb1101bea317ab8dcc33d94897b) |
| 主板集成 | [BSP `ad0747ede81ce1c2e8660e0ce1a514e6addee4d5`](https://github.com/dshanpi/aibsp-imx6ull-pro_linux5.4.47/commit/ad0747ede81ce1c2e8660e0ce1a514e6addee4d5) |
| 硬件标签 | `imx6ull-pro-integration-v0.1.0-dev.20261125.1` |
| BSP 基线 | `e00afed02481332d2de44a498518b596651dcdef` |
| 实际应用源码版本 | `0.1.0-imx6ull.20261124.3` |

设备树补丁、Buildroot 配方、显示/触摸 udev、板级启动/维护脚本及平台设备映射已整理上传到
[主板仓库 dshancontrol 目录](https://github.com/dshanpi/aibsp-imx6ull-pro_linux5.4.47/tree/ad0747ede81ce1c2e8660e0ce1a514e6addee4d5/dshancontrol)。
通用 C++ API、HMI、桌面、媒体、总线、生命周期/更新器和 Go 涂鸦网关仍由 DshanControl 维护。
主板配方以完整提交和应用源码归档哈希固定引用，不从浮动分支拉取发布输入。

## 镜像与交付附件

| 项目 | 大小 / SHA256 |
| --- | --- |
| 解压整盘镜像 | `562036736` 字节 / `a162e6f5b4600edc7ed4650c2d56d8042ceb132d1c922bdfecc58d60f4066aa8` |
| 压缩镜像 | `55978337` 字节 / `588d1d837b01f1e80c0f90352b601ee8d5d4ef409c45ebc4fc6970c79a5e135a` |

附件包含 `.img.gz`、应用源码、主板源码、BSP 基线源码、Buildroot 依赖来源/许可证、有效配置与补丁、工具链说明、测试日志、板卡配置、构建/验收记录、隐私检查、`release-lock.json` 和 `SHA256SUMS`。
镜像不是 application OTA 包，不应交给桌面应用更新器安装。

## 构建与验证

在 192.168.1.228 的独立输出从固定源码构建 U-Boot、Linux、Qt 和原生应用，复用已校验的下载缓存。
第一次构建在时区源文件组选项处失败；修正为 `BR2_TARGET_TZ_ZONELIST=asia`，本地时区仍为 `Asia/Shanghai`。
修复后保留本次新输出内已编译的依赖，重新准备干净应用快照、清理应用构建目录并重编译，完成完整镜像。
未使用 2026-11-24 的旧 target 或旧镜像。失败日志和修正记录随证据归档，没有把失败改写为通过。
原无 `.1` 候选标签保持不动，未形成镜像 Release；本交付使用 `.1` 新标签。

- 核心测试 5 项通过；仓库测试 378 项、16 项跳过。跳过项需要 root 角色隔离或隔离 SocketCAN 环境，原始原因保留。
- Qt 测试 132 项通过、0 失败；发布校验器 6 项正向/拒绝测试通过。
- 固定应用提交的 [GitHub 主机检查](https://github.com/dshanpi/DshanControl/actions/runs/45582261643)和发布工具提交的 [主机检查](https://github.com/dshanpi/DshanControl/actions/runs/45583896495)均通过；后续交付回执属于单独文档提交。
- 板级准备入口拒绝错误应用提交和未提交源码，且不创建输出目录。
- 整盘 MBR/嵌入分区、U-Boot 默认启动参数、ext4 fsck、内核/DTB 配置和关键文件哈希检查通过。
- 实际 rootfs 中的原生服务、Goodix 触摸规则及源码身份通过核验；目标镜像没有 Python 解释器和旧 Python Web 载荷。
- 实际镜像 `/etc/timezone`、`localtime` 链接及上海 TZif 数据通过核验。
- ARM 用户态模拟中的本地 API、HTTP 契约、桌面 RPC、布局持久化及 1024×600 截图通过；截图渲染结果已检查。该测试不覆盖实机内核、LCD 或触摸。

## 工厂配置与资格边界

新工厂镜像启动 `/usr` 下的原生 API 与桌面。特权平台服务和 OTA 尚未配置设备系统契约、公钥、持久存储及维护互锁；依赖这些配置的硬件 RPC/OTA 不可用。
生命周期适配器中的实验室布局检查不是通用工业维护互锁，不得复制其他设备的 CID 配置来绕过检查。
公开附件不包含实验室 Wi-Fi 密码、云凭据、签名私钥、板端数据库或备份。

新完整镜像 **未烧录或启动验证**；此前实板触摸、背光和 OTA 回退通过的记录属于另一套按文件部署的实验室环境。
物理断电矩阵、真实外部从站、官网/涂鸦业务、ARM32 语音 SDK 和长时间老化未验收。
Buildroot 自身源码由单独 BSP 归档补齐；外部 Bootlin 工具链的组件源码/许可证映射和厂商固件分发审核仍待完成，原始 legal-info 警告及获取说明均保留。
因此依据仓库 [发布门禁](RELEASING.md)保持 Draft，`auto_update_eligible=false`，不宣称生产或公开安装资格。

## 存储和安装

新整盘镜像为 8 MiB 偏移的 16 MiB FAT 启动分区加 512 MiB ext4 根分区；没有工厂 p3，默认 `/data` 位于 rootfs。
现有实验室设备为三分区布局。直接整盘写入会覆盖分区表，不能宣称保留 p3 用户数据；本轮未操作该板。
重新部署前必须核对实际 CID/布局、完成可恢复备份，并由独立维护环境完成有界写入和读回；不能覆盖挂载中的根文件系统。
安装工具入口为 [LYNX](https://github.com/dshanpi/lynx-releases)；当前 MCP 不支持 NXP 软件下载模式切换，不把既有网络/RAM 维护脚本称为通用 LYNX 安装支持。
重建命令和板级前置条件见 [硬件集成说明](https://github.com/dshanpi/aibsp-imx6ull-pro_linux5.4.47/blob/ad0747ede81ce1c2e8660e0ce1a514e6addee4d5/dshancontrol/README.md)。
