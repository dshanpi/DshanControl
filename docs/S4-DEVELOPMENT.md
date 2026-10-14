# S4 开发记录

2026-10-12 用户确认：DshanControl 是本次工业控制桌面系统主体；T153 为已有实现，
T113s4 SD NAND + Buildroot 为新增目标。EinkOS 仅参考发布标准，SmartDesktopMimi 单独开发。
此范围已经进入 AGENTS.md、发布门禁和仓库检查脚本。

## 实现

- 新增 Buildroot 201902 外部接入、S4 板级逻辑描述、隔离配置脚本、源输入哈希记录。
- 固定原 SDK Linaro 5.3.1 / ARMv7-A softfp 工具链，不使用另一块板的 NAND 配置。
- 保留现有 Qt 工业 HMI、Web、core、supervisor，修复 Python 3.7 不支持的两处 API。
- 新增隔离的 Flask 2.1.2 运行依赖及哈希锁，中文字体配方保留许可证。
- 新增应用签名包工具、离线门禁、GitHub 候选筛选/下载、维护状态检查、事务安装、健康观察、
  失败恢复和周期检查。自动安装默认关闭；信任根和维护检查由系统管理。
- 更新主机 CI。保留 T153 历史快照内容，测试同时检查快照哈希与接入脚本安装当前应用。

## 验证边界

主机测试覆盖设备身份/ABI 错配、跨产品、签名/包篡改、渠道隔离、包路径与文件类型、
重复 JSON 字段、空间不足、安装锁、升级路径/数据格式、维护状态拒绝、模拟中断恢复、
失败回退与数据保留。模拟中断不代表物理掉电验证。

S4 当前仍是开发草稿：实际屏幕/触摸、硬件修订、存储容量、实板连接尚待确认。
未生成合格烧录镜像、未烧录、未做现场总线互操作、未做实际断电恢复或长期老化。
系统契约模板保留 pending，缺少已验证身份/信任根时拒绝安装；不得改标为通过。
未检测到 ADB 或 USB 串口连接的目标板；不能用主机模拟代替上板验收。

本轮主机验证：`make test` 52 项通过（核心 5 + 仓库/应用/更新/发布 47）；
Buildroot 构建出的主机 Python 3.7 上更新器 27 项通过；锁定私有 Python 依赖下核心/Web
8 项通过。上述数字不包含任何实板测试。

## 本轮交叉构建结果

2026-10-12，Buildroot `dshancontrol` 目标成功结束（exit 0）。Qt Base/Charts
5.11.3、Python 3.7.2、私有 Python 依赖、字体和工业 HMI 已安装到目标目录。
修复了宿主 GCC 9 编译 Qt 5.11.3 qmake 时缺少 SeedSequence `result_type` 的问题；
补丁随外部 Buildroot 树保存，未直接修改 SDK 配方。

- 构建应用提交：`68784eba5bd94e156c6cfd3705256148243137e1`（干净源码快照）。
- 硬件输入提交：`0bbdbbabcfad8da15696e803f6714bfd1748a0a6`。
- HMI SHA256：`5d8c83574ee56c7394d1b58e8fd5ef1bb1aea0bc96c09cfe5810930971a53173`。
- ELF：ARMv7-A、ELF32、softfp 调用约定，解释器 `/lib/ld-linux.so.3`。
- HMI、LinuxFB 插件、Python SSL/SQLite/zlib、OpenSSL 的递归动态依赖核对通过，
  共检查 42 个文件。此检查证明依赖存在，不代表目标板上已成功加载运行。
- 使用上述原生 HMI、测试专用设备契约及临时 RSA 密钥生成签名草稿包，离线验签、
  源码/构建信息/载荷哈希检查通过；公开发布检查因缺少实板证据按预期拒绝。
  测试契约 board_id 为 `fixture-only-t113s4`，不适用于真实设备；测试私钥已删除。

本地构建及证据位于 SDK 的 `out/dshancontrol-s4-dev2/`：
`build-provenance.json` 记录输入；`validation/` 保存构建日志、测试日志、ELF 信息、
依赖清单和 `report.json`。测试草稿包位于应用仓库忽略目录 `dist/signed-draft-smoke/`。
这些产物未上传，未生成完整系统镜像，SDK 全组件清单仍未完成。

## 后续验收顺序

1. 确认实板修订、屏幕/触摸、SD NAND 容量和调试连接，完成 S4 BSP/Bootloader
   及完整 rootfs 构建，核对分区容量，补齐 SDK 组件与本地补丁清单。
2. 配置真实设备身份、公钥、维护互锁及板级健康探针，生成系统契约并打包开发镜像。
3. 上板验证冷启动、显示/触摸、网络、所需工业接口和数据持久化，再验证错板拒绝、
   签名拒绝、应用升级、实际掉电恢复、回退和老化；未通过前保持草稿状态。

## 第二轮：系统接入与持久存储（2026-10-12）

继续开发提交：`378c5c3b1bda855a37daddc0d28d2b254499d1d2`。

发现并修复 SDK 默认 `S60mount_udisk` 的不兼容行为：它尝试 FAT 初始化，且挂载失败
会尝试格式化。应用版本切换需要符号链接和持久事务，因此 S4 固定使用 ext4 数据卷。
新增系统存储检查，按 bootloader 的 `partitions=` 映射解析 UDISK，验证实际块设备、
挂载设备号、ext4 和读写状态；未知文件系统、错设备、只读或缺少挂载均拒绝写入。
启动路径不格式化、不转换已有数据。Buildroot 最后的 post-build 覆盖厂商 S60，
避免包安装顺序重新引入自动格式化逻辑。工厂 ext4 数据卷的制备尚未完成。

新增 `build_s4.py`、`capture_sdk_inputs.py`、`audit_s4_system.py`：

- 构建入口验证准备目录快照哈希，清除继承的其他板型环境，固定 S4 板级变量并留存日志。
- 记录实际 SDK 清单的 **122 个组件提交**。发现 7 个组件含本地修改或额外输入，
  已记录差异哈希与未跟踪文件信息；这些记录不是可恢复的完整源码/补丁归档。
- 核实 Tina pack 先拷贝 Linux 配置，再以 Buildroot 配置覆盖。最终 rootfs 分区为
  1 GiB，不能混用 Linux 目录里的 104 MiB 分区表；512 MiB 的 rootfs 配置可放入。
- 完整性预检检查分区名称/容量、MMC root 序号、原始 ext4 超级块及镜像截断，保留
  实际存储容量、完整 rootfs、数据卷制备、Bootloader/内核和实板测试未完成项。
- 当前基线固定分区总计 1,130,368,000 字节。预检加上 32 MiB 开销预算和至少
  256 MiB 数据空间后，容量预算下限为 1,432,357,888 字节；这不是实板容量，
  最终仍须检查厂商打包后的实际偏移、UDISK 大小和应用数据增长空间。

验证结果：`make test` **63 项通过**（核心 5 + 其余 58）；新增系统/存储测试
11 项在 Buildroot 主机 Python 3.7 上通过。新快照再次交叉构建 HMI 成功，哈希仍为
`5d8c83574ee56c7394d1b58e8fd5ef1bb1aea0bc96c09cfe5810930971a53173`。
post-build 覆盖检查通过，原生程序和预装提交一致。S4 内核 `olddefconfig` 成功；
原始 S4 board.dts 编译成功，DTB SHA256 为
`eb8ef715bc44bd5dfae1cca677df1e29bc05f7696b25a25172df4ffe83d15e59`。
此次 DTB 使用当前 SDK 的 include/dtsi 与已构建 dtc，仅作构建检查；未生成或验证启动镜像。

新准备目录：SDK `out/dshancontrol-s4-dev3/`，`validation/` 保存本轮证据。
其中 `buildroot` 链接复用 dev2 编译缓存，当前目标目录已更新为本轮版本；
第一轮源码快照、日志和测试签名包保留原版本身份，不代表当前目标目录内容。
根 SDK `.buildconfig` 和已有硬件修改未改变，未刷写或发布。

当时构建盘仅余约 **2.3 GiB**，完整 rootfs 入口按已记录的 **16 GiB 可用空间保留量**
停止启动全量构建；该阈值是工程保留量，不是测得的最小值。
当时需要提供有足够空间的构建目录，并确认实板容量、面板/触摸与连接方式。
当时 ADB 无设备、USB 未检测到目标板；完整 rootfs、内核/Bootloader 镜像和实板验收均未完成。

2026-10-13 第16轮复查空间已恢复约124 GiB，历史空间阻塞已解除；完整系统输入恢复、
隔离准备和 rootfs/镜像构建仍待执行。不得把旧 ELF/准备快照视为当前完整系统。
当前进度以 [本轮验收表](ITERATION-ACCEPTANCE.md) 为准，真实容量/硬件身份仍未确认。

## 第18轮：SDK 输入恢复与新准备目录（2026-10-13）

本轮恢复材料只保存在应用仓库外：
`/home/ubuntu/dshancontrol-recovery-20261013-h02/`。
`recovery.json` 记录122个SDK组件、嵌套squashfs仓库和独立批准硬件仓库的提交，
每个组件目录包含自包含 `baseline.bundle`、当前文件相对HEAD的二进制补丁、
未跟踪内容归档及忽略路径清单。恢复的是工作文件内容，暂存区与未暂存区的区分不作为构建输入。
7个脏组件、嵌套仓库和硬件仓库已在 `rehearsal/` 实际恢复核对；原SDK清单未变。
原 `.buildconfig` 仅作为历史输入保存，仍是其他板型配置，不能拿来打包S4。

恢复应在新目录中按组件清单从bundle克隆、检出记录提交、应用 `worktree.patch`，
再恢复未跟踪文件；嵌套squashfs仓库单独恢复至其下载路径。
解压下载和工具链归档前先核对 `recovery.json` 与本轮工具链报告中的哈希，
不要把任何归档解压到既有工作树。原始工具链压缩包在对应组件bundle中，
实际解压工具链另有逐文件、链接和权限清单。所有厂商源码、二进制和SDK缓存留在仓库之外。

这些材料解决提交/修改只有哈希、没有恢复内容的问题，但仍不是完整系统复现验收。
忽略文件清单中还包含旧板构建输出；后续完整构建须确认实际依赖，不将旧boot文件当成批准产物。
rootfs、内核、Bootloader、工厂ext4数据卷与最终打包镜像仍需构建和核验，
真实容量、板型修订、面板/触摸与正式信任根仍须取得外部证据。
本轮最终结果与日志见 [验收表](ITERATION-ACCEPTANCE.md) 和 `dist/iteration-h02-local/`。

新准备目录为 `/home/ubuntu/dshancontrol-s4-prepared-20261013-h02/`：
当前110文件应用快照、独立Buildroot输出目录，prepare/configure成功；SDK源码和工具链仍共享只读输入。
新目录未安装rootfs，完整预检因两项存储策略缺失和六个未解决条件保持未通过。
主机118 Python、32 Qt及准备快照HMI交叉构建通过，实际sysroot仍为dev2。
三套工具链21979个文件/链接与厂商压缩包内容相同；最终约94GiB可用，完整构建已另登记后续。


## 第20轮：独立系统组件构建（2026-10-13）

本轮继续使用第18轮准备目录的独立 Buildroot O 目录。构建通过 bubblewrap 将 SDK
源文件设为只读、关闭构建进程网络，并隐藏原 `out/`，只挂入已核验的交叉工具链。
下载使用准备目录内的独立副本；缺失的 DejaVu 2.37 从 Buildroot 源镜像获取并按厂商
配方 SHA256 校验。实际选中的220个包有41个本地源路径，均存在；直接 SITE 路径内未发现忽略输入，下载输入170个文件已单列哈希。
随后实际构建发现 SITE 之外的 LVGL 兄弟目录含旧对象文件，说明该检查本身不构成依赖闭包。
原122组件清单和 `.buildconfig` 核验未变。

在应用仓库外从恢复材料还原 Linux 5.4、U-Boot、SPL 和无线固件，并叠加批准硬件仓库的
125个内核文件及6个固件文件。独立内核产出 zImage、板级DTB、8个模块和903个导出头文件；
Buildroot 仅挂入本次导出的内核头文件。U-Boot 采用批准 BoardConfig 指定的
`sun8iw20p1_t113s3p_defconfig`，MMC SPL 使用显式 `mmc` 目标和独立输出路径。
该厂商配置名不解决实板 T113s3/T113s4 身份问题，也不构成实板支持声明。
SPL 使用的厂商预编译库仍是必须保留的系统输入，不能描述成全部由开放源码生成。

补发现 HEAD bundle 不包含版本生成器读取的厂商标签；从原 SDK 保存已有标签的补充
bundle 到恢复目录 `system-build-tags/`，还原后重建内核和 U-Boot。最终组件哈希在
`dist/iteration-h02-system/kernel-artifacts-final.json` 和 `bootloader-artifacts-final.json`；
早期哈希与失败构建单独保留，不能当最终版本。补充标签仅覆盖本次消费的组件元数据，
不代表所有 SDK 元数据及最终打包输入已通过完整复现验收。

真实全量编译发现厂商配方依赖其默认下载工具链选择，S4 外部配方作如下限定适配：

- sdk_memory、sdk_endec 和 sdk_camera 的具体编译/安装阶段使用 GNU C++11，满足
  `nullptr`、原子变量、lambda 和 GNU `typeof`；设置不传播到依赖包或 C 编译。
- sdk_common 的 multiscreen 示例缺少 `atoi` 声明，仅补 `cstdlib` 预包含。
- tplayer 安装阶段选择已同步的 `arm-linux-gnueabi` softfp 音频库。
- libcedarse 是 local-site 包，常规补丁阶段不会执行；通过 post-rsync hook 修正复制到
  构建目录的 Makefile，仅接纳已固定的 GCC5/arm-linux-gnueabi/EABI/glibc 组合。
  其他未知组合继续拒绝，原 SDK 文件保持只读。

真实 make/编译器测试覆盖作用域隔离、softfp目录选择、错误 ABI/工具链拒绝及恢复。
第18轮110文件应用快照未改写，本轮 external.mk/补丁作为明确记录的系统集成覆盖挂入；
后续新建准备目录会从当前仓库复制这些输入。原快照的 dirty 身份保持不变。
完整 rootfs、最终系统打包和实板验收状态以 [验收表](ITERATION-ACCEPTANCE.md) 为准；
组件构建成功不授权工厂 UDISK 制备、烧录或发布。

补充恢复材料位于恢复目录 `system-integration-round20/`，保存本轮配方/补丁和缺失的
DejaVu下载包，来源及摘要见 `dist/iteration-h02-system/recovery-supplement.json`。
只读沙箱阻止宿主ncurses安装阶段更新 `/etc/ld.so.cache`；四个生成的宿主库已实际加载验证，
无需放宽主机写权限。Python安装删除不存在的缓存目录也产生厂商忽略错误；这两类历史日志
单独保留，目标扩展与最终rootfs仍须按实际产物核验。

第9次 rootfs 尝试在 `lv_g2d_test` 暴露新的来源污染：配方复制 `../lvgl` 中153个
被 Git 忽略的旧对象文件，VPATH 据此跳过编译而链接查找失败。没有修改或清理原 SDK；
从第18轮 bundle/本地修改恢复整个 `platform/thirdparty/gui/lvgl-8`，1383个源文件核验
一致，排除旧忽略构建输出后只读挂入第10次构建。恢复记录在 `lvgl-clean-source.json`，
失败目录保存在准备目录 `failed/lv_g2d_test-attempt9`。后续复现必须使用这份干净源码
或等价恢复，不能只检查各包 SITE 目录后直接复制其未审计的兄弟目录。


第20轮最终结果：独立Qt5.11.3/HMI及512 MiB原始ext4 rootfs构建通过，镜像SHA256为
`d55ddb38b66393e09ca17217412673a94b06e2708054c84d416c50b4055b62ed`。
1091 ARM ELF的加载器/递归依赖/符号版本、两份单列的协处理器固件、只读fsck及镜像抽取25文件检查通过。
最终122 Python、32 Qt及原SDK/应用快照连续性检查通过，详见第20轮验收记录。

补齐厂商packer的两项系统输入：S4 target-finalize按 `build/mkcmd.sh:pack_buildroot_rootfs`
生成 `/init → bin/busybox`，拒绝未知原入口与越界路径；按 `copy_bin_firmware` 的覆盖顺序安装
批准100ask板级 `amp_dsp0.bin` 与 `amp_rv0.bin`，来源/哈希见 `coprocessor-firmware-inputs.json`。
它们是Xtensa/RISC-V协处理器数据，不是ARM Linux程序，也没有在本轮重新编译或实板执行。
不要将ramdisk的busybox/init脚本复制到这个no-ramdisk根文件系统。

完整镜像构建在只读bwrap中需设置 `FAKEROOTDONTTRYCHOWN=1`：旧fakeroot对未映射用户的
真实chown返回EINVAL，只模拟元数据可避免该操作。已用实际chown/lstat验证并检查镜像内
root/DBus/PostgreSQL属主，不应通过开放宿主写权限来绕过。第11次准确命令和挂载见
`rootfs-attempt11-command.json`，全部包及系统集成通过后才产生本次镜像。
Buildroot的版本标签也已补充归档与恢复验证，见 `restored-buildroot-tags.json`。

归档阶段接收AGENTS/PROMPT新增硬件交接：主交互代理持有S4实板，已观测SD容量2013265920字节、
root p5和UDISK p9 vfat，指定RGB50 1024x600与AIC8800D80。按实测容量重跑预检为零配置错误、
3项未完成（工厂ext4 UDISK、启动镜像链、实板功能/掉电/回退），publishable=false。
H02保持pending，Tina厂商完整镜像打包/静态核验仍有本地后续；不能沿用无设备/容量未知或全部外部阻塞。
Ralph只做本地构建，不与主交互代理争用硬件，不读取私有WiFi凭据；后续仍须遵守厂商系统、
网络传文件、USB厂商烧录工具及重新核对绑定板的要求。当前组件/rootfs没有新镜像实板验收或发布资格。
