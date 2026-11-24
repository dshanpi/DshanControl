# DshanControl 移植到 100ASK i.MX6ULL Pro

本适配以 [i.MX6ULL Pro 教程](https://aibsp.100ask.org/docs/imx6ull-pro/)及其公开 BSP 为依据，使用独立的 Buildroot external tree 编译 DshanControl。适配目录不修改 T113/T153 的配置，也不执行烧录。

目前属于开发适配。主机上的 Qt 界面运行、单元测试或 ARM 用户态模拟运行，都不能代替实板的显示、触摸、启动和稳定性验收。具体执行结果见本目录的 `validation.md`。

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
| 本地数据 API | Python/Flask，监听 `127.0.0.1:8080` |

BSP 内部分说明仍提到本地源码压缩包和 `-emmc.dtb`；本适配以固定提交中的真实 defconfig 为准。不要因为说明文档的旧名称而替换当前设备树、启动参数或根分区。

编译内容包括界面 `omnigate-hmi`、桌面服务 `dshancontrol-desktop-service`、通用媒体 worker、原生 runtime，以及 Web/core 本地数据服务。runtime CLI 会安装，但本适配没有配置 OTA 服务、签名身份、升级分区或硬件控制授权。

媒体 worker 没有 i.MX6ULL 解码后端。Allwinner 的 `CONFIG+=cedar` 和绑定 Qt 5.11.3 的私有图片插件不用于此板；PNG/JPEG 由 BSP 自身的 Qt 提供。

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

Qt 和 Python 会增大根文件系统，本适配把 ext4 大小设为 512 MiB。这是开发镜像配置，是否适合实板分区，仍需检查实际 eMMC 容量、最终镜像布局与启动参数。

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

布局保存在 `/data/dshancontrol/hmi/`，Web 数据库、设置和本机 session key 保存在 `/data/dshancontrol/web/`。这些目录在本开发配置中位于可写 ext4 根文件系统，**重新烧录根文件系统会覆盖其中的数据**。独立数据分区和升级迁移尚未设计完成。

运行目录为 `/run/dshancontrol/`，本地 API 日志为 `/run/dshancontrol/web/web.log`。日志随重新启动丢失。默认没有配置云端凭据、外部家庭服务或音视频服务。

为了避免争用 framebuffer，配置取消选择 LVGL demo，并用第二层 overlay 将 BSP 的 `S99lvgl9-demo` 变为无操作脚本。若启动时发现 LVGL 已在运行，DshanControl 拒绝再启动一个界面，不会强行结束该程序。

## 6. 显示与触摸适配

默认使用 `/dev/fb0`；Qt evdev 自动枚举输入设备。配置文件为 `/etc/dshancontrol/display.env`。只在确认设备对应关系后，设置 `QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS` 的 event 路径和旋转参数。

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

测试报告必须保留 skipped 数量及原因。仓库原有测试环境固定 Flask 2.1.2；此 BSP 的 Flask 3.1.3 兼容性应使用额外隔离环境检查，不能悄悄替换旧测试依赖。

| 现象 | 检查方向 |
| --- | --- |
| `Exec format error` | 是否误把 x86 主机程序放到 ARM 板上 |
| 缺少 `ld-linux-armhf.so.3` 或 GLIBC 版本 | 是否混用了其他工具链的 sysroot/rootfs |
| Qt platform plugin 无法加载 | 检查新 rootfs 中 linuxfb 插件及其依赖，开启 `QT_DEBUG_PLUGINS=1` 获取加载错误 |
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

脚本先验证 ELF 架构和 hard-float 标志，再以 Cortex-A7 用户态模拟运行 ARM Python 本地 API、ARM 桌面服务和 ARM Qt 界面；检查 API、RPC、布局初始化和截图。临时进程与数据在结束后清理，证据目录保留日志、截图和二进制哈希。它不会访问 framebuffer、串口或烧录设备，也不验证内核、驱动或启动脚本。

## 8. 实板接入前仍需确认的内容

本次明确只进行软件工作，实板尚未连接。后续上板时，仍需记录硬件版本、具体 LCD 型号、串口或网络连接信息。烧录前还需明确允许覆盖的存储介质，检查并备份已有数据。不要直接运行教程中的无人值守烧录脚本。

当前未提供实板升级契约和维护互锁，不将这个开发适配作为可安装的 OTA 包。CAN、RS485、EtherCAT、Wi-Fi、音频、视频和背光控制没有因“界面能够打开”而获得能力声明。
