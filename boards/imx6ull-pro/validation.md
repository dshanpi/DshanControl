# i.MX6ULL Pro 软件验证记录

验证日期：2026-11-24。构建与运行主机为 `192.168.1.228`，Ubuntu 24.04，8 vCPU、约 23 GiB 内存。显示目标为 1024×600。本次没有连接实板，没有烧录或访问板端设备。

## 定位构建与源码

在 228 的 Ubuntu 终端执行：

```sh
cd /home/ubuntu/dshancontrol-imx6ull-20261124
git -C source branch --show-current
git -C source log -2 --oneline
```

源码分支为 `port/imx6ull-pro-20261124`。应用适配提交为 `1591dfdf02aae15661d7c8e10abf9124c5ca2785`，基线为 `dd1f67fe1e93240c8bf55d04e7cfcdaaf809bb14`。验证工具和本报告作为后续提交保存，不改变已编译应用的源码输入。

| 路径（相对工作根目录） | 内容 |
| --- | --- |
| `source/` | 应用源码及本适配指南 |
| `bsp/` | 固定提交的厂商 BSP，工作树保持干净 |
| `application-build/external/app-source/` | 本次构建使用的应用源码快照 |
| `application-build/buildroot/` | Buildroot 输出、交叉工具链和目标暂存区 |
| `application-build/buildroot/images/` | 系统镜像及独立启动文件 |
| `application-build/provenance.json` | 准备时记录的版本、Git 状态、配置和源码哈希 |
| `snapshot-commit-verification.json` | 源码快照与应用适配提交的字节一致性记录 |

快照最初在适配代码尚未提交时生成。原始 `provenance.json` 保留当时的基线和修改状态；后续校验确认 690 个源码文件与上述适配提交一致，无差异、无额外文件。这个对应关系不把最初的工作区状态改写为干净提交构建。

## 核对构建版本

| 组件 | 本次版本或提交 |
| --- | --- |
| BSP | `e00afed02481332d2de44a498518b596651dcdef` |
| Buildroot | 2026.02.1 |
| ARM GCC / glibc | 14.3.0 / 2.41 |
| 目标 Qt | 5.15.14，Widgets、linuxfb、evdev、Fontconfig |
| 目标 Python / Flask | 3.14.4 / 3.1.3 |
| 目标 OpenSSL / SQLite | 3.6.2 / 3.51.3 |
| Linux | 5.4.47 BSP，`05967a3aaba135f40eee6eaf14ced11c090d32c1` |
| U-Boot | `65baaf3fb37946b98d7c85ad5e014f9389d580a4` |
| 主机 Qt | 5.15.13 |
| ARM 模拟器 | qemu-arm-static 8.2.2，Cortex-A7 用户态模式 |

交叉编译的四个原生程序均为 ARM32、EABI5、hard-float ELF，使用 `/lib/ld-linux-armhf.so.3`：`omnigate-hmi`、`dshancontrol-desktop-service`、`dshancontrol-media-worker`、`dshancontrol-runtime`。

## 查看主机测试结果

| 检查 | 结果 | 日志 |
| --- | --- | --- |
| `make test` | 331 项，303 通过、28 跳过，退出码 0 | `host-test-final.log` |
| `make test-hmi` | Qt 测试 130 项通过；原生 update bridge 和更新流程检查通过，退出码 0 | `host-hmi.log` |
| Flask 3.1.3 隔离环境兼容测试 | 35 项通过 | `flask31-tests.log` |
| x86 Qt 界面与本地服务运行 | API、桌面 RPC、布局初始化通过，截图显示本地数据已连接 | `host-smoke/result.json` |

28 项跳过主要涉及缺少 TLS/MQTT fixture 工具，以及要求 root、特定进程权限或固定角色身份的场景。跳过项没有计为通过；本次结果不表示这些权限与协议场景已在新板验证。

查看精简结果：

```sh
rg 'Ran [0-9]+ tests|^OK|Totals:' host-test-final.log host-hmi.log
cat host-test-final.exit host-hmi.exit
cat host-smoke/result.json
```

两个 `.exit` 文件均应为 `0`。测试套件使用仓库原有依赖；Flask 3.1.3 检查使用独立环境，没有替换仓库锁定的测试依赖。

## 复查 ARM 桌面运行

ARM 检查以构建输出中的 ARM Python 启动 API，再启动 ARM 桌面服务和 ARM Qt 界面。检查项包括 API 响应、Unix socket RPC、布局文件初始化、进程存活和 1024×600 PNG 截图。

在 228 运行以下命令，输出目录需使用一个尚不存在的新路径：

```sh
python3 source/scripts/smoke_imx6ull_qemu.py \
  --target application-build/buildroot/target \
  --output arm-smoke-recheck
cat arm-smoke-recheck/result.json
```

成功时退出码为 0，`passed`、`local_api`、`desktop_rpc`、`layout_created` 均为 `true`。`desktop-arm.png` 应显示完整桌面、正常中文和“本地数据已连接”。脚本结束时关闭本次启动的进程并删除临时数据，保留日志和证据文件。

第一次 ARM 运行发现缺少 Fontconfig，日志提示“无法初始化搜索框字体渲染配置”。适配已加入 Qt Fontconfig 支持、Fontconfig 运行库和 DejaVu 字体，重新构建后 `arm-smoke-fontconfig/result.json` 检查通过。原始失败证据保留在 `arm-smoke/`，没有覆盖。

用户态模拟使用 Ubuntu 主机内核和 Qt offscreen 输出。它验证 ARM 程序与运行库能够协同运行，不验证新内核启动、linuxfb 输出、触摸输入或实板性能。模拟器日志中的动画预算提示不能换算为实板帧率。

完整构建结束后的最终运行证据位于 `arm-smoke-final/`，上述四项结果均为 `true`。此目录记录的四个应用程序哈希与镜像校验从根文件系统提取的程序哈希一致，截图尺寸为 1024×600，已经人工查看。

## 检查完整系统镜像

完整 Buildroot 构建退出码为 `0`，见 `image-build-result.json`。以下文件位于 `application-build/buildroot/images/`：

| 文件 | 字节数 | 用途 |
| --- | ---: | --- |
| `u-boot-dtb.imx` | 527360 | 带设备树的 i.MX 启动文件 |
| `zImage` | 7868256 | Linux 内核 |
| `imx6ull-14x14-evk.dtb` | 37741 | 本次适配的设备树 |
| `rootfs.ext2` | 536870912 | 实际格式为 ext4 的 512 MiB 根文件系统 |
| `boot.vfat` | 16777216 | 16 MiB FAT 启动分区 |
| `sdcard.img` | 562036736 | 536 MiB 完整开发镜像 |

`sdcard.img` 的 SHA-256：

```text
80a5965f8b89b3d1f9454bbfabb2f1e19e04b3bb857aaf0b5cae66a194c96036
```

只读镜像检查已通过，证据位于 `image-validation/`：

```sh
cat image-validation/result.json
cat image-validation/e2fsck.log
cd application-build/buildroot/images
sha256sum -c ../../../image-validation/SHA256SUMS
```

`result.json` 中 `passed` 为 `true`，`e2fsck -fn` 返回 0，六个镜像文件的哈希检查均通过。重新执行完整检查时，回到工作根目录并选择新的证据目录：

```sh
python3 source/scripts/verify_imx6ull_image.py \
  --buildroot-output application-build/buildroot \
  --output image-validation-recheck
```

已核实的镜像内容：

- U-Boot 位于镜像偏移 1024 字节处；长度未覆盖其配置的环境区 `0xE0000`。
- MBR 第 1 分区从 8 MiB 开始，大小 16 MiB；第 2 分区从 24 MiB 开始，大小 512 MiB。两个分区的内容与独立生成的文件完全一致。
- U-Boot 编译默认值为 `mmcdev=1`、`mmcpart=1`、`mmcroot=/dev/mmcblk1p2 rootwait rw`、`console=ttymxc0`，包含本次 DTB 的选择逻辑。
- FAT 分区内的内核和 DTB 与独立文件逐字节一致。
- ext4 不启用 `64bit`、`metadata_csum_seed`、`orphan_file`，只读完整性检查通过。
- 四个应用程序、linuxfb/offscreen 插件、Fontconfig、DejaVu 字体、启动脚本、平台描述均已进入最终根文件系统；用户 UID/GID、video/input 组和脚本执行权限符合适配配置。
- LCD 和 GT9XX 坐标属性均为 1024×600；USDHC2 为 4-bit、50 MHz，并设置 `no-1-8-v`。厂商的 LCD timing 节点名称保留了旧命名，实际分辨率以 `hactive`、`vactive` 属性为准。

上述检查读取普通镜像文件，没有挂载真实磁盘，也没有启动镜像中的内核。

## 确认未覆盖的功能

本适配启用本地桌面和本地数据服务。运行时数据保存在开发版 ext4 根文件系统中的 `/data/dshancontrol/`，重新写入根文件系统会覆盖这些数据。

| 项目 | 当前状态 |
| --- | --- |
| 1024×600 桌面、中文、本地 API、桌面 RPC | 已完成软件运行检查 |
| 内核启动、eMMC 读写、显示与触摸、背光、冷启动和长期稳定性 | 等待实板验收 |
| 视频解码 | 未集成 i.MX6ULL 后端，媒体 worker 不提供解码能力 |
| OTA | 未配置板端身份、签名、升级分区与维护互锁，未启用 |
| CAN、RS485、EtherCAT、Wi-Fi、音频、家庭服务与云端接入 | 没有宣称新板验证通过 |

后续上板验收时，应先记录板型、LCD 型号、串口日志和存储布局，再执行启动、显示、触摸和服务检查。此次软件结果不能作为生产发布或实板升级完成证明。
