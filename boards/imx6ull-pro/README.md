# DshanControl 移植到 100ASK i.MX6ULL Pro

2026-11-25 起，板级代码的维护入口为 [BSP 仓库 dshancontrol 目录](https://github.com/dshanpi/aibsp-imx6ull-pro_linux5.4.47/tree/master/dshancontrol)。
设备树/Buildroot/启动和维护脚本已上传，硬件标签 `imx6ull-pro-integration-v0.1.0-dev.20261125.1`
固定引用应用 `ab981454fd095fb1101bea317ab8dcc33d94897b`。本目录保留该版重建和既有验证记录；
后续板级修改在 BSP 仓库维护，通用 C++/Go 服务仍在 DshanControl。

本适配以 [i.MX6ULL Pro 教程](https://aibsp.100ask.org/docs/imx6ull-pro/)及其公开 BSP 为依据，使用独立的 Buildroot external tree 编译 DshanControl。适配目录不修改 T113/T153 的配置，也不执行烧录。

目前属于开发适配。2026-11-24 已通过网络传输、独立 RAM 维护环境和 `dd` 完成实板 p1/p2 更新，并通过正常重启后的桌面、API、Wi-Fi 自动启动检查。详细过程见 [网络烧录实板记录](network-dd-verification.md)；早期主机验证记录保留在 `validation.md`。2026-11-25 用户已确认触摸点击及位置正常，见 [原生服务验收](native-services-validation.md)。物理断电与长时间稳定性尚未验收；新完整镜像需要独立验证。

## 1. 版本与适配范围

| 项目 | 使用的输入 |
| --- | --- |
| BSP | `100askTeam/aibsp-imx6ull-pro_linux5.4.47` |
| BSP 提交 | `e00afed02481332d2de44a498518b596651dcdef` |
| Buildroot | `2026.02.1` |
| 架构 | ARMv7-A，Cortex-A7，hard-float |
| 工具链 | defconfig 选择的 Bootlin `armv7-eabihf--glibc--stable-2025.08-1`，GCC 14 系列 |
| Linux | `05967a3aaba135f40eee6eaf14ced11c090d32c1`，5.4.47 BSP |
| U-Boot | `65baaf3fb37946b98d7c85ad5e014f9389d580a4` |
| DTB | defconfig 实际选择 `imx6ull-14x14-evk.dtb` |
| 显示 | Qt5 Widgets、linuxfb、Qt evdev |
| 本地数据 API | C++ / QtNetwork / SQLite，监听 `127.0.0.1:8080` |

BSP 内部分说明仍提到本地源码压缩包和 `-emmc.dtb`；本适配以固定提交中的真实 defconfig 为准。不要因为说明文档的旧名称而替换当前设备树、启动参数或根分区。

编译内容包括界面 `omnigate-hmi`、桌面服务 `dshancontrol-desktop-service`、软件媒体 worker、原生 runtime，以及 Web/core 本地数据服务。2026-11-24 已在当前实验室实板配置受限总线执行层、背光、媒体和签名开发应用升级，详见[完整服务教程](../../docs/guide/imx6ull-services.md)与[实板验证记录](full-services-validation.md)。这些单板开发配置不自动授予其他设备升级资格。

媒体 worker 使用 `CONFIG+=software` 的 FFmpeg 软件后端。Allwinner 的 `CONFIG+=cedar` 和绑定 Qt 5.11.3 的私有图片插件不用于此板；PNG/JPEG 由 BSP 自身的 Qt 提供。

## 2. 准备构建主机

教程建议 Ubuntu 24.04、至少 8 GB 内存和 100 GB 可用空间。完整 BSP、Qt、根文件系统和最终镜像会同时占用空间，应在开始前检查构建目录所在文件系统：

```sh
df -h /path/to/work
free -h
nproc
```

不要直接清理其他工程来腾出空间。应用组件单独构建可以先行，但也需要预留工具链、Qt 源码和中间文件的容量；不能把“应用编译成功”写成“固件构建成功”。

常用主机依赖：

```sh
sudo apt-get update
sudo apt-get install build-essential git rsync cpio unzip bc file wget curl \
  xz-utils bzip2 patch perl python3 python3-venv libncurses-dev libssl-dev \
  bison flex
```

主机 Qt 测试另需：

```sh
sudo apt-get install qtbase5-dev libsqlite3-dev zlib1g-dev
```

## 3. 固定 BSP，生成独立配置

在新的工作目录克隆 BSP，避免操作已有产品工程：

```sh
git clone https://github.com/100askTeam/aibsp-imx6ull-pro_linux5.4.47.git bsp
git -C bsp checkout e00afed02481332d2de44a498518b596651dcdef
```

从含有本适配的 DshanControl 源码根目录执行：

```sh
python3 scripts/prepare_imx6ull.py \
  --bsp /absolute/path/to/bsp \
  --output /absolute/path/to/new-output
```

输出路径必须不存在，且位于应用源码和 BSP 目录之外。脚本会检查 BSP 提交及 Buildroot 目录的修改状态，复制应用源码，创建 external tree，合并配置并执行 `olddefconfig`。`provenance.json` 记录应用 Git 状态、BSP 版本、配置哈希和源码文件哈希；有本地改动时会如实记录，不能据此宣称是正式发布源码。

目录结构：

```text
new-output/
├── provenance.json
├── external/
│   ├── app-source/          # 本次构建的应用源码快照
│   ├── package/             # DshanControl 包
│   └── overlay/             # 显示配置、设备权限与 LVGL 启动切换
└── buildroot/
    ├── .config
    ├── host/                # 交叉工具链和主机工具
    ├── target/              # 目标文件系统暂存区
    └── images/              # 完整构建完成后产生镜像
```

应用快照在准备时固定。修改原始源码不会自动进入已有快照；正式复现时应使用新的输出目录重新准备。

## 4. 先编译应用，再构建完整系统

只构建应用及其依赖：

```sh
CCACHE=none CCACHE_DISABLE=1 make -C /absolute/path/to/new-output/buildroot -j6 dshancontrol-imx6ull
```

成功后，程序位于 `buildroot/target/usr/bin/`。此时只完成包安装，最终用户表、overlay、根文件系统和磁盘镜像可能尚未生成，不能直接把暂存区当作可烧录系统。

在有足够空间的主机上构建完整镜像：

```sh
CCACHE=none CCACHE_DISABLE=1 make -C /absolute/path/to/new-output/buildroot -j6
```

Buildroot 根据固定的 Git 版本下载 Linux 和 U-Boot，并保留 BSP 原有内核补丁。完整构建预期输出 `u-boot-dtb.imx`、`zImage`、所选 DTB、ext4 根文件系统及 `sdcard.img`。应以本次成功构建的日志和文件哈希确认产物，不能仅凭文件名判断。

Qt、字体及媒体库会增大根文件系统，本适配把 ext4 大小设为 512 MiB。当前板级包使用原生 API、总线执行器和生命周期工具，不选择 Python 运行依赖。增量 Buildroot 构建不会自动删除旧 target 中已安装的 Python 文件；生成无旧依赖的新系统镜像时应使用干净输出目录。最终镜像是否适合实板分区，仍需检查 eMMC 容量、布局与启动参数。

下载缓慢时可在 Buildroot 配置中设置官方镜像 `BR2_PRIMARY_SITE="https://sources.buildroot.net"`。所有下载仍应通过包中记录的哈希校验，不替换未知来源的工具链或关闭校验。

`CCACHE=none` 避免 SQLite 的配置程序自动使用主机 PATH 中的 ccache；`CCACHE_DISABLE=1` 避免误用已有项目留下的缓存。在 228 的实际构建中，旧缓存目录属于 root，未采用这些设置时会报权限错误；本适配没有修改那个目录。

## 5. 启动、进程身份与数据目录

新镜像中的 `S99dshancontrol` 按顺序启动本地 API、桌面服务和界面：

| 进程 | UID | 说明 |
| --- | --- | --- |
| Web/core API | 400 | 仅绑定 loopback 的 8080 端口 |
| Qt HMI | 401 | 使用 video/input 组访问显示和输入设备 |
| 桌面服务 | 402 | 通过 Unix socket 提供桌面数据 |

界面通过 `DSHANCONTROL_HMI_API_PORT=8080` 连接本机 API。新参数只允许有效端口号，主机固定为 `127.0.0.1`；没有设置时仍使用旧板的 80 端口。

默认健康检查也使用同一端口；启动管理器显式设置 `HEALTH_URL` 时保留其覆盖行为。HTTP 路由、字段、错误返回和桌面 RPC 的完整对照见 [后端接口说明](../../docs/backend-api.md)。

布局保存在 `/data/dshancontrol/hmi/`，Web 数据库、设置和本机 session key 保存在 `/data/dshancontrol/web/`。`S35dshan-data` 优先挂载已有的独立 p3；保留数据包带有 `require-data-partition` 标记，p3 不可用时拒绝启动应用。没有该标记的两分区开发镜像使用根文件系统保存数据，重新烧录会覆盖这些数据。此次保留了原 p3 文件，没有自动迁移旧应用数据库格式。

运行目录为 `/run/dshancontrol/`，本地 API 日志为 `/run/dshancontrol/web/web.log`。日志随重新启动丢失。默认没有配置云端凭据和外部家庭/语音服务。音频由 `S97dshan-audio` 初始化，受管 runtime 由 `S98dshan-runtime` 启动；配置应用升级后 `S99dshancontrol` 使用 `/data/dshancontrol/application/current`。

为了避免争用 framebuffer，配置取消选择 LVGL demo，并用第二层 overlay 将 BSP 的 `S99lvgl9-demo` 变为无操作脚本。若启动时发现 LVGL 已在运行，DshanControl 拒绝再启动一个界面，不会强行结束该程序。

## 6. 显示与触摸适配

默认使用 `/dev/fb0`；Qt evdev 自动枚举输入设备。Goodix 屏由 `71-dshancontrol-goodix.rules` 修正为 touchscreen，并创建 `/dev/input/dshan-touchscreen`；实板 event0 是触摸，event1 是 GPIO 按键，不应固定套用编号。配置文件为 `/etc/dshancontrol/display.env`。只在确认设备对应关系后，设置 `QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS` 的 event 路径和旋转参数。

本次用户确认屏幕为 1024×600，与组件桌面的最小布局一致。BSP 原始 LCD 时序已配置为此分辨率；本适配修正原设备树中仍为 800×480 的触摸坐标范围，保留控制器配置写入和固件更新关闭。触摸驱动使用这两个属性设置输入坐标范围，原 Goodix 配置数据本身也是 1024×600。

内核补丁还采用 BSP 自带 eMMC 指南中记录的 USDHC2 兼容参数：4-bit、禁止切换到 1.8 V、最高 50 MHz。补丁通过 external tree 叠加在原有内核补丁之后，不修改 BSP checkout。这些配置依据不能代替本镜像的实板测试。

在实板接入后，先读取真实设备信息：

```sh
uname -a
cat /proc/device-tree/model
cat /proc/fb
cat /sys/class/graphics/fb0/virtual_size
for f in /sys/class/input/event*/device/name; do echo "$f"; cat "$f"; done
df -h
```

随后验证屏幕完整显示、边缘点击、多点/拖动、旋转、冷启动、进程退出行为及持续运行内存。主机生成的截图只用于界面检查，不证明这些项目通过。

## 7. 主机验证与问题定位

在 Ubuntu 上执行：

```sh
make test-bootstrap
make -j6 test
make -j6 test-hmi
mkdir -p dist/hmi
cd dist/hmi
qmake ../../components/hmi/src/omnigate-hmi.pro
make -j6
QT_QPA_PLATFORM=offscreen ./omnigate-hmi --desktop-self-test
QT_QPA_PLATFORM=offscreen ./omnigate-hmi --desktop-render ../desktop-render
```

测试报告必须保留 skipped 数量及原因。仓库保留 Python 作为主机回归驱动及旧版本合同参照，原测试环境固定 Flask 2.1.2；这些依赖不属于新原生应用包。Go 网关测试需要 Go 和 SQLite 开发库，`make test` 同时执行网关的 race 检查与真实 Qt HTTPS 联调。

| 现象 | 检查方向 |
| --- | --- |
| `Exec format error` | 是否误把 x86 主机程序放到 ARM 板上 |
| 缺少 `ld-linux-armhf.so.3` 或 GLIBC 版本 | 是否混用了其他工具链的 sysroot/rootfs |
| Qt platform plugin 无法加载 | 检查新 rootfs 中 linuxfb 插件及其依赖，开启 `QT_DEBUG_PLUGINS=1` 获取加载错误 |
| `无法初始化搜索框字体渲染配置` | 检查 Qt Fontconfig 支持、`libfontconfig.so.1` 和 DejaVu 字体是否进入目标根文件系统；本适配已将这些项目纳入依赖 |
| 有 framebuffer 但黑屏 | 检查实际屏幕时序、驱动、背光、设备权限和是否有另一 GUI 占用 |
| 有画面但触摸不响应 | 检查实际 event 设备、evdev 权限、坐标范围及旋转 |
| 布局目录不可用 | 检查 `/data/dshancontrol/hmi` 是否由 UID 401 所有且可写 |
| 本地数据未连接 | 检查 8080 API、Web 日志、数据库目录权限和 manifest |
| 视频提示不可用 | 当前没有 i.MX6ULL 解码后端，不能安装 Allwinner Cedar 库来解决 |

### ARM 用户态运行检查

应用包构建成功后，可在已安装 `qemu-user-static` 的主机上运行：

```sh
python3 scripts/smoke_imx6ull_qemu.py \
  --target /absolute/path/to/new-output/buildroot/target \
  --output /absolute/path/to/new-evidence
```

该脚本对应迁移前的 ARM Python API 冒烟检查，不能作为当前原生应用包的验收依据。原生版本需执行 `make test`、`make test-hmi`，再检查 ARM ELF、依赖、实际板端进程、API 与 GUI 心跳。临时主机进程或 QEMU 截图不能替代内核、驱动、启动和重启验证。

### 检查完整镜像

完整构建返回 0 后，在同一主机执行：

```sh
python3 scripts/verify_imx6ull_image.py \
  --buildroot-output /absolute/path/to/new-output/buildroot \
  --output /absolute/path/to/new-image-evidence
```

检查脚本使用 Buildroot 的主机工具及 Ubuntu 的 `e2fsprogs` 工具，只读检查普通镜像文件，不挂载、不访问块设备。证据目录必须不存在。成功标志为退出码 0，且 `result.json` 中 `passed` 为 `true`。

检查内容包括 MBR 分区及偏移、嵌入镜像的字节一致性、FAT 分区中的内核/DTB、ext4 完整性与特性、根文件系统中的程序/显示插件/字体、服务用户及设备组、启动脚本权限和设备树参数。`SHA256SUMS` 可用于复制后校验：

```sh
cd /absolute/path/to/new-output/buildroot/images
sha256sum -c /absolute/path/to/new-image-evidence/SHA256SUMS
```

这些检查不启动镜像内的 Linux 内核。内核启动、init 流程和显示触摸驱动仍需后续上板验收。

## 8. 实板接入与保留数据的更新

2026-11-24 后续维护中，用户明确授权网络传输后执行 `dd` 并正常重启。串口确认 i.MX6ULL、512 MiB 内存和 1024×600 显示；已在 RAM 环境卸载 p1/p2、验证回滚备份、写入并逐分区读回校验。首次启动发现的等待超时和 Wi-Fi 状态映射问题已通过文件修复解决，之后正常重启自动启动成功。授权范围记录在根目录 `AGENTS.md`。

实板采用 32 MiB 启动分区、768 MiB 根分区和独立的 256 MiB `/data`。本指南前述 `sdcard.img` 是新介质用的两分区开发镜像，不能整盘覆盖这块有数据的板子。保留数据的镜像准备、无线启动流程及验收边界见 [Wi-Fi 与保留数据更新](wifi-preserved-data.md)。

源码适配目录不是 OTA 包；本轮另行制作并验证了签名开发应用包。每次硬件操作仍需适用的实时维护互锁；网络/RAM 维护不涉及 NXP USB 模式切换。Wi-Fi、framebuffer、本地 API、背光、软件媒体、总线回环/模拟及应用回退的实际结果见[服务验证记录](full-services-validation.md)。外部从站、真实云业务、语音和物理听感保持待验收。
