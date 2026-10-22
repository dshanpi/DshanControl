# DshanControl 双发布线与代码门禁

本项目是工业控制桌面应用系统，沿用现有 T153 应用实现，新增 T113s4 SD NAND
Buildroot 接入。EinkOS DSP-LINUX-001 1.1.0 仅作为分层和发布标准参考；
SmartDesktopMimi 不在本项目范围内。本约定来自 2026-10-12 用户确认。

2026-10-14 的代码现状、缺口、统一门禁与分阶段调整建议见
[独立应用发布落实方案](APPLICATION-UPDATE-PLAN.md)。其中待实施项不代表现有能力。
该方向已获用户确认；持续执行
[硬件操作与应用更新门禁](HARDWARE-AND-UPDATE-GATES.md)，包括固定任务/作用域、
资源锁、严格 SSH 信任、真实阶段/错误投影及失败不自动重试。
应用持久事务恢复/已知良好回退仍须执行，不能与重发失败任务混淆。

## 来源和边界

S4 底层使用 DongshanPI/100ASK_T113s4-SdNand_TinaSDK5 的 t113_s4/100ask，
不能替换成 SPI NAND 配置。T153 支持保持原状。通用应用不得写死新板设备节点。
A133、T527、RK3576、IMX6ULL-PRO、STM32MP157、RK3506 尚未接入，不能标记支持。
板级样例不等于经过验证的系统镜像，后续目标见 BOARD-PORTING-QUEUE.md。

参考规范：https://github.com/dshanpi/EinkOS/blob/main/standards/linux-board-release/v1.1.0/SPEC.md

## 独立源码预览（2026-10-22）

源码预览使用 source-* 标签，只交付干净已提交 Git 树、校验值和明确边界的主机验证记录。GitHub 可标为 prerelease，但它不是上表 application/system 发布，不交付安装清单、签名应用包或固件，不设置 Latest，也不得交给板端更新器。其发布不会改变现场系统、维护许可、签名信任、安装身份或硬件资格。生产/应用/系统的完整门禁仍保留。

make package 仅从已提交 Git 对象导出，不包含工作目录未跟踪文件；已有输出不覆盖。发布前检查公开内容、测试跳过和未验收项，上传后逐附件下载核验。需求原件和批准图保留，主机渲染与板端/物理验收分别声明。

## 两条发布线

| 类型 | 内容 | 必须记录 |
| --- | --- | --- |
| system | BSP、Buildroot 基础库、系统服务契约、更新器、公钥、首次预装应用 | 硬件完整提交、SDK 所有组件及本地补丁/未跟踪输入、配置、工具链、契约、预装应用提交、镜像哈希、实板证据 |
| application | DshanControl 桌面、Web、核心/业务服务、应用资源 | 应用完整提交、版本、源码及目标包哈希、目标 ABI、系统契约、升级路径、数据格式、验收记录、签名 |

应用标签使用 app-vX.Y.Z；系统标签使用 system-vX.Y.Z--BOARD。
应用更新不强制新硬件提交或镜像。系统原始 build-info 不随应用更新改写。
应用必须匹配经验证的系统契约，不追随硬件分支的最新提交。

## 自动门禁

清单格式见 platform/contracts/application-update-v1.json。新设备运行端使用
components/runtime 的 C++ 校验；主机打包器/历史参考使用 Python 协议实现，通过原生 CLI
与故障回归验证一致行为。GitHub Actions 安装 Qt/Charts/OpenSSL/zlib 后运行主机、Qt 和仓库门禁。

1. release_kind、app_id、仓库、渠道、完整提交和目标资产全部明确；禁止 GitHub Latest 盲选。
2. 逐项匹配 board_id、hardware_revision、architecture、float_abi、libc、runtime_id、
   contract_id、updater_protocol。每个目标包仅覆盖一个明确系统配置。系统必须明确声明
   allowed_channels 和匹配的 updater_version；缺失值不得默认成 stable。公开发布证据中的
   系统契约也须通过同一产品、渠道和更新器校验，并与目标身份及数据格式配对。
3. RSA/SHA-256 签名覆盖清单原始字节及包哈希；公钥由系统配置，不从下载包建立信任。
4. 只允许应用版本目录中的普通文件/目录；拒绝链接、特殊文件、路径穿越、重复项和安装脚本。
5. 安装锁、空间检查、持久日志、原子切换、开机恢复、健康观察和失败版本隔离。
6. 用户数据独立保存。v1 仅允许相同数据格式，禁止携带迁移钩子；不支持的迁移明确拒绝。
7. 检查更新与安装分离；安装必须经过系统拥有的维护状态检查。工业现场未进入可停机状态不安装。
8. 防重放序号单调递增；本机故障回退不降低已接受序号。回退失败必须留下事务以便继续恢复。
9. S4 应用/数据卷必须是启动分区映射指定的可写 ext4 UDISK；拒绝 FAT、错设备和只读卷。
   启动失败不得触发格式化。完整 rootfs 的 post-build 必须覆盖厂商自动格式化脚本。
10. 系统打包前检查 Buildroot 实际采用的分区表、MMC root 分区序号、原始 ext4 镜像容量
    和真实介质容量。SDK 提交清单、修改哈希与可恢复的源码/补丁交付物分别记录。

## 验收与公开发布

Draft 可记录 not_run；Prerelease 和 Stable 都必须通过签名/错板拒绝、包边界、
断电恢复、健康回退、数据保护及工业维护状态检查。Stable 还必须完成所有宣称功能。
每个目标记录实际系统镜像和契约哈希、目标包哈希、测试者、时间与日志，不能填写无证据的 passed。
首次镜像须验证：首次启动 → 应用升级 → 重启 → 故障回退 → 用户数据完整。
CAN/RS485/EtherCAT 区分软件可用、接口枚举和外部从站互操作；未接设备不能视为互操作通过。

系统签名公钥配置、实板断电/恢复、屏幕和触摸、现场总线及老化未完成前，S4 只允许开发草稿。
签名证明来源，不证明兼容或质量；签名包仍需全部安装门禁。应用发布器不上传/公开 Release。
公开流程需要重新下载附件校验，并冻结标签与资产。源码包工具不等于应用更新包工具。

## 当前实现边界

当前源码、测试、CI 和脱敏实板记录已同步开发分支；实板第6版通过完整签名包安装、
健康原码/回退、两处物理断电和临时系统冷启动验证，详见
[实板继续验证](validation/2026-10-14-board-native-ota-continuation.json)。
本轮试验工具源码归档在 boards/t113s4-sdnand/validation/20261014-native-pilot，
不被应用载荷/正式 post-build 自动安装。fixture 契约和故障注入只能用于复核试验；
不作为正式基线或生产维护回调，也不因代码上传生成公开 Release。

新增 S4 接入与应用更新 v1 是开发实现；主机测试结果见工作记录，不能宣称生产 OTA 就绪。
已有 T153 短测状态保持原义，不代表新增更新器已在 T153 验证。底层和更新器自身通过系统线升级。
development_mode 仅接受 JSON 布尔值，省略时按生产门禁拒绝草稿。主机 CLI 进程终止与恢复
测试验证事务行为，不替代实板掉电、存储持久性或正式签名/维护互锁验收。

打包器要求 native-build-info 绑定当前干净应用提交、HMI 哈希和完整目标契约，
不接受把另一提交/架构的二进制重命名后发布。构建记录随清单一起签名绑定。

For T113s4 vendor system images, the final IMAGEWTY container must also pass
`scripts/check_s4_boot_header.py` with the hardware-information bytes generated
by the matching vendor buildserver/update_chip configuration. See
`boards/t113s4-sdnand/hardware-inputs.md`. Missing information can pass ordinary
flash checksums yet panic in OP-TEE, so a successful USB flash is not boot
acceptance. Application releases do not carry or change Boot0.

## 2026-10-14 压缩系统草稿交付

用户已明确授权上传 SDK 差异与镜像。执行
[S4 系统交付说明](S4-SYSTEM-DELIVERY-20261014.md)：完整版本配对、压缩、私密材料检查、
草稿附件上传及逐项回下载校验。原始含共享私钥镜像不上传；脱敏副本单列哈希与未重新
实板验证状态。不因上传而提高 OTA 资格，也不重新烧写现有板上试验环境。

## 2026-10-15 原生系统开发草稿

后续独立构建/脱敏/压缩交付见
[S4 原生系统交付](S4-NATIVE-SYSTEM-DELIVERY-20261015.md)。
C++ 系统服务进入新工厂镜像；完整回下载核验与实板验收分别记录。
新镜像尚未实板烧录，正式身份/签名/维护互锁和 OTA 资格仍待验收。
