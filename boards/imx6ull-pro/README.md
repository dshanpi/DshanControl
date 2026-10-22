# DshanControl i.MX6ULL Pro Lite 移植

此接入以 100ASK i.MX6ULL Pro、512 MiB、7 寸 RGB LCD 1024×600 为目标。应用来源为 `feat/qt5-component-desktop-preview`，固定提交见 `source-identity.json`。使用 SDK 的 Linux 5.4.47、eMMC U-Boot 与 Buildroot 2026.02.1；不修改 T153 或 S4 的板级文件。

## 获取本分支

```sh
git clone --branch 6ull https://github.com/dshanpi/DshanControl.git
cd DshanControl
```

本分支已包含桌面和软件视频修改，无需再次 `git apply`。构建入口保留在
`boards/imx6ull-pro/`。SDK/内核/Bootloader 的来源仍固定；不将厂商二进制、
完整 SDK、镜像、设备账号、Wi-Fi 密码或私钥放入应用 Git 仓库。

`source-identity.json` 保留已经实板验证的 preview 基础提交和补丁 SHA256，
不是对本分支最新提交已烧录的声明。`prepare-sdk.py` 对照该基础校验应用内容，
在 SDK 的 `sources/dshancontrol-build-identity.json` 生成实际 HEAD、基础提交、
分支和 `application_dirty`。干净分支提交记录为 false；未提交开发修改明确为 true。
`build.sh` 将实际提交和生成记录交给 Buildroot 版本目录及安装器，新构建不再
冒用旧基础提交作为当前应用提交。更新固件后才会改变板端身份；现有验收镜像不改写。

旧的独立补丁包仍可在固定 preview 基础提交上还原历史应用树，但不能将它再应用到
本分支。详细的历史实板范围和未完成门禁见
[6ULL 开发验收](../../docs/IMX6ULL-DEVELOPMENT-20261022.md)。

## 构建

```sh
git clone https://github.com/100askTeam/aibsp-imx6ull-pro_linux5.4.47.git
cd aibsp-imx6ull-pro_linux5.4.47
git checkout e00afed02481332d2de44a498518b596651dcdef
git submodule update --init --recursive
SDK="$PWD" OUTPUT=/path/to/imx6ull-build /path/to/DshanControl/boards/imx6ull-pro/build.sh
```

`prepare-sdk.py` 校验两个 SDK 子模块提交及工作树，从固定 Git 输入生成本地源码归档并记录 SHA-256，随后从 `defconfig.in` 生成 SDK 构建配置。首次需要完整主机依赖和下载上游包；代理环境变量由调用方按自己的网络设置。构建输出包括 U-Boot、zImage、DTB、rootfs.ext4、data.ext4、sdcard.img 和 SHA256SUMS。

## 板级选择

- Linux RGB 时序沿用 SDK 的 1024×600；触摸范围改为 1024×600，按 Goodix 名称绑定 evdev。
- 本板 512 MiB 内存的 CMA 从旧多媒体配置 320 MiB 调整为 64 MiB。背光 LUT 调整为 0..255 连续等级，保持原 PWM 周期和默认最大亮度，匹配桌面的逻辑亮度接口。
- eMMC 使用 SDK 指南推荐的保守 4bit、50 MHz、no-1-8-v；不复制 Allwinner DTS 或分区表。
- 启动分区为第 1 分区，rootfs 为 `/dev/mmcblk1p2`，持久 ext4 数据为第 3 分区 `/data`。整个镜像大小必须小于实测 eMMC 容量。数据文件系统在 Buildroot fakeroot 中生成，以保留实际 root 所有权和私有文件权限。
- Web 和 Qt 沿用上游实现；平台和组件后台使用上游 C++ 服务。当前开发版服务以 root 运行；没有声称已完成生产权限隔离。
- 背光使用平台 C++ sysfs 接口，绑定 `backlight`，由驱动 `max_brightness` 换算逻辑值。
- 当前仅声明经本板读取的显示、触摸和管理网口；CAN、Modbus、EtherCAT、蜂窝须有独立驱动、接线及从站验收后再启用。
- 采用匹配当前 Qt 的标准图像格式插件，不加载只适用于 Qt5.11.3 的 S4 私有图像插件。媒体 worker 使用便携构建，不冒充已具备 Allwinner Cedar 硬件解码。
- 应用、布局、配置及数据库位于 `/data/dshancontrol`；媒体目录是 `/data/media`。更新私钥和设备凭据不进入镜像。未配置生产签名公钥，自动安装关闭。
- ADB 使用固定序列号 `100ask_IMX6ULL`；串口为 `ttymxc0`、115200。烧录通过本板 LYNX NXP/UUU 通道，UART 仅用于命令与启动日志，不传文件。

## 验证

`source-identity.json` 是输入追溯，不是实板通过声明。正式结论应以本次构建日志、镜像哈希、LYNX 任务终态、串口启动日志、ADB 结果、API 检查和真实 framebuffer 为准。云端授权、语音、现场从站、生产 OTA 和长期老化仍继承上游 preview 的独立验收边界。

The RTL8723BU USB module uses standard linux-firmware Realtek 87xx Wi-Fi/Bluetooth payloads and wireless-regdb. rng-tools feeds the on-chip /dev/hwrng. UUU ends with FB: done; power-cycle separately only after the flash task reaches success, since a fastboot reset disconnects USB before UUU receives its acknowledgement.

Qt linuxfb 只使用主 framebuffer，关闭额外占用 32 MiB 的 MXC overlay。cfg80211/mac80211/rtl8xxxu 使用标准模块装载，使驱动在根文件系统可用后读取签名法规库；补入同一上游法规库的公开签名证书，保持 REQUIRE_SIGNED_REGDB 开启。SDK 的 SDMA 在早期根文件系统挂载前请求固件会回退重试，该日志须与真正的固件缺失区分。

本次实板冷启动发现 RTC 回到 1970，会令 Web 永久会话 Cookie 过期。S05dshanclock 在 Web 启动前恢复构建时间下限或 /data 保存的上次时间；每 5 分钟及正常关机保存。该值只是离线时间估计，不代表准确计时；增加标准 ntpd，接通可访问 NTP 的网络后校时。离线精确时间仍需要有效 RTC 备用电源或人工校准。时钟采用 UTC。

## 初版镜像的设备验收记录（2026-10-21）

LYNX NXP/UUU 已完成成功烧录（退出码 0），支持串口 `bmode usb` 后的 SDP 识别、电源通道 7 冷启动及正常系统重启。`board-readback.py` 可核对引导程序、完整启动 FAT、四个应用二进制、ABI来源身份、Qt 心跳和组件服务；不比较启动后会变化的 root/data 全文件系统。LYNX 没有厂商介质 verify 能力，任务字段 verifyState 不能用于声称该项通过。

实板 Web 登录/CSRF/设备和点位元数据 CRUD、背光 180→重启保留→恢复 220 已验证。测试元数据清理后交付。Goodix 驱动已识别，但物理点按需要现场确认； framebuffer 截图只是实际渲染结果，不能代替现场光学显示和触摸检查。网口本次无载波、无 DHCP 租约；不声称联网、NTP 实际同步、云授权、语音、音频、现场总线和长时间运行通过。保留 SDK 的早期 SDMA 固件回退及未用外设警告，完整串口日志随验收资料保存。

LYNX MCP 的 `lynx_adb_shell` 本次出现超时，使用主机原生 adb 完成板端检查；LYNX 的 ADB 设备识别、串口、固件传输、烧录及电源通道分别记录。

`set-admin-password.py` 是开发板凭据设置工具：经 ADB 传入后运行，会在板上生成独立随机 Web 初始密码，仅修改 /data 内管理员认证配置。不要把其输出写入源码、固件包或公开日志。

2026-10-22：实物显示与触摸已由用户确认；增加 wpa_supplicant/iw/ntpq 和 S41wifi。Wi-Fi 配置仅从板上私有的 /data/dshancontrol/network/wpa_supplicant.conf 读取（root:root，0600），不在镜像内保存用户 SSID/PSK。运行总览原本按数组前两项选本地/无线链路，在双以太网加 Wi-Fi 时错误；补丁改为按 kind 匹配并优先已连接的同类接口。source-identity 记录基础提交和补丁 SHA256，未声称补丁已进入上游提交。

2026-10-22 增补：Wi-Fi DHCP、网关、DNS、校验证书的 HTTPS 和 ntpq 同步状态已在实板验证；12 个桌面应用入口已打开。组件服务 37 项本地/未配置边界检查、Qt 88 项测试和完整 host test 通过。总览网络补丁与 Wi-Fi 用户态程序采用增量方式部署。本日新打包全量镜像只完成构建与静态检查，没有再次全量烧录；不能复用初版烧录回执为新镜像声明通过。用户网络凭据不随源码或镜像交付。

## 6ULL 软件视频后端（2026-10-22）

本板没有视频解码 VPU；PxP 不是压缩视频解码器。Buildroot 集成 GStreamer 1.24.13、gst-libav 和精简 FFmpeg H.264/MPEG4/MJPEG 解码库。仅本板 media.pro 启用 CONFIG+=gstsoftware，保留原 Cedar 和不带解码器的构建。

视频 worker 使用 playbin + appsink 软件解码，Qt 通过私有 Unix socket 接收 RGB565 帧并绘制，不直接写 framebuffer。通道按 worker PID/UID 验证，传输接收边界为 640×480；本板性能配置实际输出不超过 320×240，并按视口和原比例缩小，带代际编号和背压丢帧，旧帧与非法数据会被拒绝。暂停、定位、循环、EOS、隐藏暂停、全屏沿用现有协议。此轮仅验证画面与控制，音频明确关闭，音量和静音控件禁用；没有声称已适配音频。

建议先使用 H.264 Baseline、yuv420p、320×240 或 640×360、15 fps 本地 MP4；更高分辨率及帧率必须重新实测，输出尺寸限制不代表可实时解码任意大分辨率源文件。演示片由 FFmpeg testsrc2 生成，无外部影片素材。

运行时文件位于 /data/media，真实播放状态可在开发配置启用时读取 /run/dshancontrol/media-status.json（仅 root，0600）；不提供远程媒体控制接口。该诊断记录不属于新生产权限隔离或签名 OTA 验收。

## 已完成的全量视频固件验收与分支提交

2026-10-22 后续已将视频完整镜像通过 LYNX 实际烧录，并完成重新上电、25 项读回、
配置/媒体恢复、再次正常重启、Wi-Fi/DNS/HTTPS、11 项视频 GUI 控制检查。用户确认
“屏幕和视频正常，暂停/继续触摸准确”。前文未再次烧录及触摸待确认的描述保留为
当时记录，最新范围以 [结构化验收](../../docs/validation/2026-10-22-imx6ull-development.json)
为准。板型仍为开发候选，`supported_profiles` 为空，自动更新未获资格。

源树提交时补齐了从干净分支构建的来源记录及候选板登记；这些构建工具/元数据整理
尚未形成新的全量烧录镜像。已烧录的镜像哈希和当时基础提交加补丁身份保持原义。


主机门禁包含 `make test`（6ULL 来源和镜像边界回归）、`make test-hmi`（三套 Qt）
与 `make test-media-software`（真实 GStreamer 软件 worker、生成的本地片段及播放控制）。
软件视频门禁需要 Qt、pkg-config、GStreamer 开发包、libav/good 插件和 FFmpeg；产物
只写入忽略的 `dist/`。它不代替 ARM 交叉构建或新的完整镜像实板资格验收。
