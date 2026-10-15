# T113s4 原生系统开发草稿交付 · 2026-10-15

本次交付包含 C++ 系统服务/更新器、S69 启动、真实 PID readiness 和恢复等待。
工厂应用与集成源码固定为干净提交 `12359ec3ceaec23640fa64764c4c09dc94ce56ef`，工厂序号 0。
与 2026-10-14 原工厂镜像及板上第 6 版试验分别记录；此前交付保持原样。

草稿地址：[GitHub Draft](https://github.com/dshanpi/DshanControl/releases/tag/untagged-2aa4ddf5ffc20538f17c)。
标签 `system-v0.1.0-dev.20261015--t113s4-sdnand` 固定上述源码提交；未公开发布。
全部 17 个远端附件已回下载，逐项 SHA256、清单、归档文件及
压缩镜像解压后的哈希/长度核验通过，详见
[结构化交付回执](validation/2026-10-15-native-system-delivery.json)。

## 构建与镜像检查

新内核、U-Boot、MMC Boot0 和厂商容器所需 FES/NAND 支持项从恢复的选定源码构建；
Buildroot 使用新的目标输出。复用固定工具链、厂商预编译库/固件和校验过的源码下载；
三次应用包开始前的源码刷新保留了同一本次输出内的依赖。旧 SDK out 被隔离，LVGL
源码中的旧对象通过新副本隔离。未对全部 122 个组件做全新检出，也未宣称完整 SDK
可复现验证。附件包含基线锁、可恢复的九组件差异、有效配置、实际冻结源码和构建配方。

厂商 MMC 参数要求 `init=/init`。正式 post-build 验证目标 BusyBox 后建立
`/init -> bin/busybox`，拒绝异常入口；源模板、实际 ext4 链接和 init applet 均纳入检查。
主机测试 161 项、Qt 测试 35 项通过，源提交
[CI](https://github.com/dshanpi/DshanControl/actions/runs/38692741910) 通过。

最终 IMAGEWTY 已核对全部 41 项、实际容量的 GPT/MBR/下载图、
Boot0 硬件信息/校验和、TOC、内核/DTB、匹配 vmlinux 调试归档和 ext4。
DTB 重定位按本次原始内核、压缩 ELF 的 BSS/重定位符号与栈/堆空间计算，
并与未修改的厂商函数核对。新压缩内核比旧镜像小 272 字节，地址相应由旧镜像的
`0x4113f100` 调整到 `0x4113f000`；旧镜像检查器的固定地址不能用于新构建。
这一检查修正没有改变镜像。

已逐字节核对目标目录与实际 rootfs 的 102 个关键文件，包括原生服务、关键动态库、
LinuxFB/触摸插件、Python 解释器与关键扩展、8 个模块及固件。新 rootfs 的副本已移除厂商共享 FreeRDP 私钥/证书并清零数据块，
fsck 和隐私检查通过；原始含共享密钥 rootfs 留在本地。公共 GnuTLS 自测向量保留。
FreeRDP 启用前另行生成每设备密钥。

| 交付项 | 长度 / SHA256 |
| --- | --- |
| 解压后的脱敏整机镜像 | 1426203648 字节 / `dc62a4b5195ffbe34af80a3e74522d15d18fb26e82b9c24058a69d37ab5b3c73` |
| 压缩镜像 | 174557548 字节 / `ebbb1d0b917d22a76bcd87883f08141324c86dd21e2bafe8acbaa7e56611892e` |

解压前先核验附件 `SHA256SUMS`；版本、工厂源码、原始 rootfs 与脱敏派生物的配对见
`release-lock.json`。两个源码归档各有逐文件 `FILE-HASHES.json`。

## 尚未验收

新镜像未实板烧录；本轮没有板卡操作，第 6 版环境与数据未改动。Gateway/SDK 隧道
连接不可用，现场状态以回执和后续实时响应为准。完整工厂镜像包含空 ext4 UDISK，
烧录覆盖现有应用/用户数据；先网络备份并验证，再重新识别板卡和 USB 绑定后执行厂商烧录。

硬件修订、正式 libc/runtime 契约、生产签名根和工业维护互锁仍待完成；自动安装关闭，
允许的本地客户端仍为 root。已有 Python Web/supervisor/health/storage 依赖仍在。
实板冷启动、屏幕/触摸/亮度、WiFi、完整签名 OTA/错板拒绝/掉电恢复/回退/数据保护、
最小权限、剩余 Python 迁移、老化和外部总线互操作均按各自证据验收。
`automatic_updates_qualified=false`，保持开发草稿。
