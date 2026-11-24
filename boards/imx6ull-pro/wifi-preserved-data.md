# i.MX6ULL Pro：Wi-Fi 与保留数据更新

本页记录 2026-11-24 接入实板后的补充适配。主机为 `192.168.1.228`，工作根目录为 `/home/ubuntu/dshancontrol-imx6ull-20261124`。首次实际 `dd` 镜像应用输入为 `c73dc9d96947fe5d38944dedf1dc96fc8a601046`；启动修复和最终离线镜像输入为 `039d4c5fa6e1ac5f3bee4108cf100a9ad99b9a70`。原有后端接口对齐修改包含在这些输入中。实际安装路径、读回结果和版本边界见 [网络烧录实板记录](network-dd-verification.md)。

## 1. 先区分已观察到的硬件与新镜像验证

串口连接 `COM51`，115200/8N1。启动日志确认 i.MX6ULL rev1.1、512 MiB 内存、1024×600 LCD 与 GT911 触摸。旧系统为 Linux 5.4.47，根设备为 `/dev/mmcblk1p2`。

旧系统的 RTL8723BU 无线网卡使用 `rtl8xxxu` 和 `rtlwifi/rtl8723bu_nic.bin`。`wpa_cli` 报告已连接用户授权的 `Programmers` 网络，状态 `COMPLETED`，地址 `192.168.1.56`。地址由 DHCP 分配，后续启动需要重新查询，不能当成固定地址。

上述识别先在原系统完成。后续新系统已真实启动，正常重启后自动联网、桌面和 API 检查通过；1024×600 framebuffer 与 Goodix 设备已确认，手指点击和拖动尚未实测。

## 2. 实板存储布局与写入边界

eMMC 用户区为 3,959,422,976 字节，每扇区 512 字节。

| 分区 | 起始 LBA | 扇区数 | 容量 | 用途 |
| --- | ---: | ---: | ---: | --- |
| p1 | 16384 | 65536 | 32 MiB | FAT 内核与 DTB |
| p2 | 81920 | 1572864 | 768 MiB | ext4 根文件系统 |
| p3 | 1654784 | 524288 | 256 MiB | ext4 `/data` |

普通开发 `sdcard.img` 仍是 16 MiB 启动分区加 512 MiB 根分区的两分区布局。它用于新介质，不能直接刷到当前实板。

保留数据方案只替换 p1/p2，不写 MBR、U-Boot、环境区、eMMC 硬件启动分区或 p3。p1 生成 32 MiB FAT 镜像；p2 从已验证的开发 rootfs 扩展至 768 MiB，并加入 `/etc/dshancontrol/require-data-partition`，禁止在缺失 p3 时回退到根文件系统保存用户数据。

## 3. 无线软件与启动次序

Buildroot fragment 增加以下选项：

```text
BR2_PACKAGE_WPA_SUPPLICANT=y
BR2_PACKAGE_WPA_SUPPLICANT_NL80211=y
BR2_PACKAGE_WPA_SUPPLICANT_CLI=y
BR2_PACKAGE_IW=y
BR2_PACKAGE_LINUX_FIRMWARE=y
BR2_PACKAGE_LINUX_FIRMWARE_RTL_87XX=y
BR2_PACKAGE_LINUX_FIRMWARE_RTL_87XX_BT=y
BR2_PACKAGE_WIRELESS_REGDB=y
BR2_PACKAGE_RNG_TOOLS=y
```

`nl80211` 选项自动引入 libnl。内核已有 `CONFIG_RTL8XXXU=m`，固件由固定版本的 linux-firmware 包安装，保留其许可证。已在 Cortex-A7 用户态模拟器中运行目标程序，版本为 wpa_supplicant/wpa_cli 2.11、iw 6.17；这项检查仅证明程序及其动态库可以加载。

启动顺序如下：

1. `S21rngd` 启动随机数服务；`S35dshan-data` 检查并挂载已有 p3。挂错设备、只读或挂载失败时不生成就绪标记，也不格式化分区。
2. `S41wifi` 只有在存储就绪后才读取 `/data/dshancontrol/network/wpa_supplicant.conf`，启动 WPA 与 DHCP 客户端。
3. `S99dshancontrol` 同样要求存储就绪，然后启动本地 8080 API、桌面服务及 Qt HMI。API 最多等待 60 次，进程提前退出则立即失败，避免原来 10 次等待导致首次启动超时。

平台 manifest 将 `wifi0` 映射到 `wlan0`，后端从实际 operstate 和 IP 地址生成网络状态。不要因为 WPA 已连接就手工把界面状态写成在线。增量开启 linux-firmware 子选项后，要执行 `linux-firmware-rebuild` 再生成镜像；仅 `reinstall` 会复用旧的 `br-firmware.tar`。

Wi-Fi 配置必须是 root 所有、权限 `0600` 的普通文件；父目录必须为 root 所有且不能被组或其他用户写入。配置文件及父目录不能用符号链接替代。不把密码放入镜像、Git、示例配置、日志或公开交付包。新镜像复用 p3 上的私有配置，不重新生成已有密码。

新桌面使用 `/data/dshancontrol/hmi` 与 `/data/dshancontrol/web`，分别由 UID 401 和 400 使用。旧系统的 `apps`、`data`、`desktop`、`provider` 等目录保留。此次未把旧数据库或设置自动迁入新的服务目录，保留文件不等于完成数据格式迁移。

## 4. 生成保留数据的分区包

先完成正常构建，并运行 `scripts/verify_imx6ull_image.py` 得到 `passed=true` 的报告。打包程序会再次核对输入文件哈希，拒绝使用验证后被改动的文件。

```sh
python3 scripts/package_imx6ull_preserved_data.py \
  --buildroot-output /absolute/path/to/buildroot \
  --validated-image-report /absolute/path/to/image-check/result.json \
  --cid '<本次从串口读取并核对的32位十六进制eMMC CID>' \
  --output /absolute/path/to/new-preserve-data-package
```

该命令只操作主机上的普通文件，不连接设备、不烧录。输出包括 `boot-preserve.vfat`、`rootfs-preserve.ext4`、`preserve-data.uuu`、`SHA256SUMS` 和 `manifest.json`。

UUU 脚本依据此 BSP 实际启用的 `CONFIG_FSL_FASTBOOT=y` 编写。该实现从 DOS 分区表生成 `mmcsdb1`、`mmcsdb2`，不能假定分区名是 `boot` 或 `rootfs`。脚本在写入前读取并检查 MBR 签名、三个分区的类型/起点/长度以及空的第四分区；任意检查失败应中止。

镜像按 16 MiB 上限分批转换为 sparse 传输，避免直接信任 U-Boot 广告的 1 GiB 下载缓冲区。当前板只有 512 MiB RAM。该方案不使用 bmap 跳过旧扇区；工具若强制启用 bmap，必须先修正受审查的通道配置。

在不访问设备的情况下检查脚本语法：

```sh
cd /absolute/path/to/new-preserve-data-package
uuu -dry preserve-data.uuu
sha256sum -c SHA256SUMS
```

已使用 Ubuntu 的 UUU 1.5.141 通过 `-dry` 检查。语法检查不证明实板 U-Boot 的命令、内存地址和分区映射已现场通过。

## 5. 备份与 MCP 烧录门禁

本次维护授权已写入根目录 `AGENTS.md`，包括连接 Wi-Fi、进入受支持的编程模式、烧录和启动验证。该记录不代替实时硬件互锁。

先核对串口板型、eMMC CID、存储布局和实际 USB 绑定。备份应存放在受保护的私有目录，包含引导区、启动分区、根分区回滚材料和用户数据。在线备份可以用于预检查，但应用仍在修改数据库，因此正式写入前必须停止应用并取得一致性备份。临时备份访问应限制为只读命令，完成后撤销。

本次 `http://127.0.0.1:8771/mcp` 的 NXP 能力报告 `softwareBootTransitionAllowed=false`，没有 NXP 专用切换工具。用户随后明确选择网络传输、`dd` 和正常重启，本次已按该路径完成；不再等待 MCP 模式切换功能。若以后使用 UUU，仍需重新读取能力并核对保存的配置，不能使用旧项目的 Allwinner/T736 参数。

UUU 包从已经确认的 Fastboot 状态开始，不负责切换 SDP/Fastboot 或改写持久 U-Boot 环境。本次网络维护保持 Linux 正常启动路径：PID 1 切入 tmpfs，真正卸载旧根后才允许写 p1/p2；未进入 USB 下载模式，也未修改其保护设置。`maintenance/` 下的脚本针对本次旧 S80 系统与固定布局，不能原样重复运行到当前 S99 系统。

## 6. 写入后验收与证据

当前 MCP 报告不支持烧录校验。任务显示成功后，仍需在受支持的恢复环境中读回 p1/p2 并核对哈希；应在第一次可写挂载根分区之前比较整分区哈希，否则正常启动日志等写入会改变结果。同时确认 MBR、保留区域及 p3 未被烧录脚本改写。

正常启动后依次检查：

```sh
uname -a
cat /proc/cmdline
cat /proc/mounts
cat /run/dshancontrol-storage-ready
wpa_cli -i wlan0 status
ip -4 address show dev wlan0
wget -q -O - http://127.0.0.1:8080/api/health
wget -q -O - http://127.0.0.1:8080/api/hmi/v1/snapshot
```

再验证服务实际 UID、目录权限、1024×600 framebuffer、触摸输入、桌面交互和冷启动后的 Wi-Fi 恢复。不要为了截图而跳过硬件互锁，或将模拟器截图称作实板照片。

早期证据在 `wifi-alignment/`：`image-check/` 为镜像检查，`arm-smoke/` 为 ARM 用户态模拟，`storage-startup-tests.json` 为 11 个隔离 chroot 场景，`uuu-dry-run.log` 仅为 UUU 语法检查。最新证据在 `network-flash/board-evidence/` 和 `network-flash/coldboot-final/`，分别保存真实板端检查与最终构建/镜像检查；旧报告中的“未烧录”是历史状态，不回写成实板通过。
