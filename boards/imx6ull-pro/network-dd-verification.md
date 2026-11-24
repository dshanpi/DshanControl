# i.MX6ULL Pro 网络烧录与实板验证记录

2026-11-24，已按用户授权完成“网络传输 → RAM 维护 → `dd` 写分区 → 读回 → 正常重启”。新系统运行 DshanControl，后续正常重启无需人工执行启动脚本。本文记录这块实板的开发验证，不表示所有业务能力或生产可靠性已经验收。

## 1. 实际对象和版本

| 项目 | 实际结果 |
| --- | --- |
| 板型 | 100ASK i.MX6ULL Pro，i.MX6ULL rev1.1，512 MiB RAM |
| 连接 | COM51，115200；无线 RTL8723BU / rtl8xxxu |
| 显示 | `/dev/fb0`，1024×600，32 bpp，stride 4096 |
| 触摸 | Goodix 驱动及 event0 已识别，未做手指点击验收 |
| 新内核 | Linux 5.4.47，`#1 SMP PREEMPT Tue Nov 24 05:37:40 EST 2026` |
| 构建主机 | Ubuntu `192.168.1.228` |
| 当前 DHCP 地址 | `192.168.1.56/24`，不是静态地址承诺 |
| 实际 dd 镜像的应用输入 | `c73dc9d96947fe5d38944dedf1dc96fc8a601046` |
| 后续文件修复及最终构建输入 | `039d4c5fa6e1ac5f3bee4108cf100a9ad99b9a70` |

源码、配置、维护脚本在 Ubuntu 工作目录 `/home/ubuntu/dshancontrol-imx6ull-20261124/source`。文档后续提交可与应用输入提交不同，交付清单分别记录。密码、私有 profile、数据库、板端备份不进入源码或公开交付包。

## 2. 分区边界与备份

目标 eMMC 为 `/dev/mmcblk1`，运行前重新核对 CID 和布局。p1 为 32 MiB，p2 为 768 MiB，p3 为 256 MiB；起始 LBA 分别为 16384、81920、1654784。

本次只替换 p1/p2，保留 MBR、U-Boot、环境区、eMMC 硬件启动分区和 p3。普通两分区 `sdcard.img` 没有写入整盘。

先完成只读备份，再在 tmpfs 中启动独立 PID 1，移动必要挂载并真正卸载旧根。演练确认可以正常返回旧系统后，才进入正式维护。正式备份在应用全部停止、p1/p2 未挂载时生成，压缩包解压哈希与原分区一致；`/data` 文件清单在维护前后相同。

板端一致性备份位于 `/data/dshan-maintenance-20261124/rollback/`：

- `rootfs.ext4.gz`、`boot.vfat.gz`：旧根分区与旧启动分区。
- `emmc-prefix.gz`：前 8 MiB 引导区域。
- `data.tar.gz`：排除维护目录后的原用户文件。
- `original-raw-hashes` 与 `data-files-*.sha256`：原始内容校验依据。

备份保存在同一 eMMC，不能当作介质损坏时的异机备份；早期在线备份另在本地主机私有目录。回滚分区同样要求独立维护环境，禁止在运行中的根分区上直接覆盖。

## 3. 传输、写入与读回

固件经局域网传入 p3，整包及每个文件均做 SHA256 校验。进入 RAM 后再次检查目标 CID、三个分区的起点和长度、PID 1 所在环境以及 p1/p2 未挂载状态。使用静态 ARM BusyBox 解压、执行 `dd` 并同步写入。

首次可写挂载前的真实读回哈希：

| 分区 | SHA256 |
| --- | --- |
| p1 | `87ede55f15743f3f39ad4c56568d9decc5e701551b7b01f2a885dccc9766ea3b` |
| p2 | `19556d23f193b58f0dab4e38c9708d60e32759c42eb79bfa0a3df36f6aaddde6` |

日志包含 `QUIESCED_BACKUP_VERIFIED`、`ROOT_PARTITION_READBACK_PASSED`、`BOOT_PARTITION_READBACK_PASSED` 和 `FLASH_AND_READBACK_PASSED`。前 8 MiB 引导区域及原用户文件校验未变。此后根分区正常挂载和文件修复会改变整分区哈希，不能再把运行中 p2 与上述值直接比较。

`maintenance/stage-ram.sh` 针对此次旧 S80 系统设计。当前系统已经改为 S99，且回滚目录已有内容；这些脚本是本次维护的可审计记录，不是可以无条件重复执行的通用 OTA 工具。

## 4. 首次启动问题与修复

首次启动时 API 的 10 次等待提前结束，随机数池约 23 秒才就绪，应用未自动启动。随机数就绪后手动启动可以成功。修复增加 rng-tools 6.17 及依赖，API 等待上限增加到 60 次，并在进程已退出时提前失败。

Wi-Fi 实际已连接，但桌面显示“链路未连接”。原因是 i.MX6ULL 平台 manifest 仅登记 display，后端返回空网络列表。增加 `wifi0 → wlan0` 映射后，API 返回真实 `operstate=up` 与 DHCP 地址，桌面显示“链路已连接”。

补入 RTL87xx 蓝牙固件和 wireless-regdb。linux-firmware 必须执行 `rebuild`，只执行 `reinstall` 会复用旧固件 tar。实板已观察到 `rtl8723b_fw.bin` 加载成功；这不等于蓝牙应用功能已验收。

后续文件修复包为 `coldboot-repair.tar.gz`，SHA256：

```text
06a629a87ad06e9d3c2981d2a680c278d771a27ec74db492b96040c0c7061b3b
```

共 30 个文件/链接，包含启动脚本、rngd、必要新库、平台配置与无线固件。安装前核对已存在的 libc/OpenSSL 依赖哈希，保留被替换文件，逐文件校验并原子替换；没有对挂载中的根分区再次执行 `dd`。记录位于板端 `coldboot-repair/installed.json`。

交付核对时发现，重新编译的 HMI 二进制哈希与初次烧入版本不同；桌面服务、runtime 与 Python API 哈希一致。为使交付程序与实测程序逐字节相同，另生成 `application-alignment.tar.gz`，包含最终构建的 HMI、桌面服务与 runtime。安装前检查旧哈希、保存原文件，再停止应用并原子替换。该包 SHA256 为 `669a8fd8fdc0fe703dc813669507f9fd95d600d2fda67474a08be8e383ec05f6`；最终 HMI 哈希为 `6f78e943a84f6056b1309e8927e0fd5ba9dd9ea34852061732a6c55a14826404`。板端记录在 `application-alignment/installed.json`。

## 5. 重启后的真实检查

修复后正常执行 `reboot`。只在系统到达登录提示后登录并检查，没有手动启动 DshanControl。验证时 uptime 为 44.92 秒，boot ID 为 `409f53b9-2c1c-4b30-a601-a1140f02f698`。

| 检查 | 结果 |
| --- | --- |
| API、Qt HMI、桌面服务自动启动 | 通过 |
| `/data` 挂载与就绪标记 | `/dev/mmcblk1p3`，`emmc-p3` |
| WPA/DHCP 自动恢复 | `wlan0` 在线，192.168.1.56/24 |
| 12 项真实 HTTP 请求 | 全部符合预期，非法输入和未确认硬件操作返回 400 |
| 设置修改与恢复 | 通过，原设置已恢复 |
| Web/HMI/Desktop 身份 | UID 400/401/402，CapEff 均为 0 |
| 以 HMI UID 连接桌面 RPC | 通过 |
| 安装修复文件哈希 | 全部一致 |
| 真实 framebuffer 输出 | 中文、1024×600 布局、网络状态及本地数据连接已检查 |

截图从真实板卡 `/dev/fb0` 采集，经 stride 和颜色位定义解码；它不是模拟器截图，也不是相机拍摄。原始 framebuffer SHA256 为 `1631b5a8a4e7ac23e789fe6ea05adcfa387509726c870ac96f45fb69fa657117`。

## 6. 交付与验证边界

最终分区包根据 `039d4c5` 重新构建，已通过镜像内容、e2fsck、设备树、依赖库及隐私配置排除检查。它包含本次文件修复，适合后续重新部署前审查。实际板端经历的是“旧镜像 dd + 启动/无线修复 + 最终应用二进制同步”，没有再次把最终重建分区包整包烧入；交付清单保留这一区别，并核对最终实板程序和修复文件与分区镜像中的文件哈希。

最新证据在工作根目录下：

- `network-flash/board-evidence/`：维护日志、HTTP/RPC/权限检查、启动信息及 framebuffer。
- `network-flash/coldboot-final/`：构建日志、源码输入、修复清单、只读镜像检查和 32/768 MiB 分区包。

当前检查是正常重启，不是切断电源后的冷启动或长期老化。触摸驱动枚举通过，但手指点击、拖动和边缘坐标尚未实测。CAN、RS485、EtherCAT、音视频、云端业务与通用 OTA 未验收，未配置的桌面卡片保留真实状态。

5.4 BSP 内建 cfg80211 在根文件系统就绪前请求 regulatory.db，仍可记录延迟加载失败；本次 WPA/DHCP 工作正常，没有把放入数据库文件描述为完整解决内核加载时序。生产网络法规区域与数据库加载应另行验证。开发 manifest 的保守验证标志也不作为整板生产认证。
