# 本轮验收进度

需求来源：`../requirements.md`。开始时间：2026-10-13。
当前阶段（2026-10-14）：C++ 更新核心、独立任务与亮度接口已实现，HMI 接通原生检查/下载/确认安装。153 项主机测试及仓库门禁、35 项 Qt 测试、S4 交叉构建及最新远端 CI 通过。实板开发试验完成完整 HMI/Web 签名包安装、健康原码/失败回退、prepared 和 switched 两处真实断电恢复、真实 Qt 检查按钮→原生任务、冷启动/WiFi 自动 DHCP 和原生亮度控制；当前运行序号 6。使用独立 fixture 契约、复制的用户数据及临时系统启动层。正式硬件身份/基线、GitHub 下载安装闭环、工业互操作、完整掉电矩阵及老化仍未验收。下表为当前范围，执行记录保留原历史。

| ID | 状态 | 现有证据 / 本轮待做 |
| --- | --- | --- |
| S01 | passed | 第17轮修复S4适配器目录引用、当前/历史UI及存储/发布文档边界，旧构建身份明确归档；118 Python（含T153接入与真实更新CLI失败/恢复）、32 Qt、独立S4构建通过，115个历史板级/身份输入与HEAD一致。证据 dist/iteration-s01-boundaries/validation.json；S4仍未实板验收，未来板型不声明支持 |
| S02 | passed | 第 13 轮按 5 张参考图复核 14 界面、五尺寸 Qt 图及全部基准空态/离线；修复 CAN 滚动、离线色、图例、字号/触控、设置图标与更新信息层级。逐页差异及能力边界见 dist/iteration-s02-s03-visual/visual-review.json，仅本机视觉，不代表实板 |
| S03 | passed | 第 13 轮五尺寸 ×14 界面、页面/表格无横向滚动、基准无页面纵向溢出；300 个主按钮/输入控件达到44×44，表格行44px；逐控件滚动后完整可见，SDO确认取消及总览告警入口真实点击通过。800总览纵排、CAN单一页面滚动；证据 dist/iteration-s02-s03-visual/validation.json |
| S04 | passed | 第 7 轮以当前源码重新验证缺测/单位/健康语义、60 秒边界、时钟倒退/恢复及暂停隔离；80 Python、21 Qt、实际 HTTP→Qt/SQLite 六次采样和独立 S4 构建通过；证据 dist/iteration-s04-window-recovery/validation.json。仅为本机软件行为，不代表实板采集已接入 |
| S05 | passed | 第 6 轮 CAN/NMT/SDO 证据 dist/iteration-s05-can/validation.json；第 8 轮 RS485/EtherCAT 参数、能力/角色映射、确认撤权、真实 Qt POST→HTTP 回放、配置保留及命令失败/恢复通过。83 Python、23 Qt、独立 S4 构建，证据 dist/iteration-s05-serial-ethercat/validation.json。串口仅保存首个从站配置，生效状态未验证；无实板互操作证据 |
| S06 | passed | 第 9 轮按板级映射分类、选择消失清空、蜂窝能力/配置与确认后复核；86 Python、25 Qt、19 次真实 HTTP 失败恢复及独立 S4 构建通过。证据 dist/iteration-s06-network/validation.json；DHCP 配置/信号不虚构，链路 UP 不视为拨号验证；无实板拨号证据 |
| S07 | passed | 第 10 轮修复大 ID、筛选/详情与确认一致性、失效门禁、重复确认时间、事务审计及 CSV 固定可见记录；89 Python、28 Qt、18 次实际 HTTP（含 Qt→Flask→SQLite）与独立 S4 构建通过。证据 dist/iteration-s07-alarms-audit/validation.json；告警/日志分别为最近 500/200 条的本机行为，不代表实板验收 |
| S08 | passed | 第 11 轮修复首次快照/保存期间编辑隔离、非预设刷新值、背光部分失败提示、服务失败审计与确认后门禁、更新结果/身份缺失显示；92 Python、30 Qt、19 次真实 HTTP（含 Qt→Flask 设置/背光）及独立 S4 构建通过。证据 dist/iteration-s08-settings-services-updates/validation.json；NTP/蓝牙无可靠运行状态契约时显示未知，GUI 安装继续禁用，无实板更新证据 |
| S09 | passed | 第 14 轮复核遗留门禁并修复公开发布证据漏检渠道/更新器版本；109 Python 及 T153/仓库门禁、31 Qt、38 次真实 CLI 调用（含两次 SIGKILL、维护拒绝、健康失败/回退恢复、锁、篡改与错配拒绝）、独立 S4 构建通过。证据 dist/iteration-s09-recovery/validation.json；仅本机协议行为，临时签名/合成证据不代表正式发布或实板掉电 |
| S10 | passed | 第 15 轮修复字体缺失字形、T153 许可源路径与字体清单安装；112 Python、32 Qt、S4 独立构建、52 ELF 递归依赖/符号版本、字体逐字节重建及真实 ARM 载荷预装/解包通过。证据 dist/iteration-s10-runtime-resources/validation.json；dev3 复用 dev2 sysroot，仅静态目标依赖审计，未在板上加载。宿主 Python 3.7 无 sqlite3，API 检查使用系统宿主 Python + 固定目标纯 Python 依赖，不替代目标执行 |
| S11 | passed | 第 16 轮核验 122 个 SDK 组件（7 个脏组件）、批准 S4 板级输入与最后 post-build；修复存储策略篡改漏检、env-5.4.cfg 覆盖顺序和固定 UDISK 容量检查，post-build 拒绝符号链接父目录。主机/CLI失败恢复、Qt和独立S4构建证据 dist/iteration-s11-system-inputs/validation.json。仅主机审计；第16轮哈希清单不是可恢复输入包，后续恢复材料/独立构建/实测容量见第18/20轮；完整厂商镜像及新镜像实板仍未完成 |
| S12 | passed | 第19轮当前源码重验：118 Python、32 Qt、独立S4 HMI构建、52 ELF依赖和diff通过；304张当前Qt图覆盖五尺寸14页，参考/空态/离线复核及300控件量测通过；当前HTTP/虚拟CAN/系统CLI失败恢复归档。核验12份历史索引及2723产物，源码差异单列；dist/iteration-s12-final/validation.json 和 software-matrix.json。不代表系统镜像或实板验收 |
| H01 | in-progress | 实板报告 allwinner,t113_s4/armv7l，SD 2013265920 字节，RGB50 1024×600；真实硬件修订及 S3/S4 商品命名关系未确定。GT911 0x5d/PB2/PB3/I2C2 engine 有驱动及事件证据，J14 位置未独立核验；不声明 CAN0 与触摸同时可用 |
| H02 | in-progress | 修正 Boot0 厂商硬件信息后的开发镜像已烧录、校验并启动；当前 ac593abd7666393c2b7e554ea8d775b7255fb7c75e8868b8f6c505159ae233bc，任务 flash-1791957654587823900，ext4 UDISK 自动挂载。原 2da05c73 镜像因 OP-TEE 错误拒绝再烧。正式信任根/身份及可完整复现的系统输入尚未完成 |
| H03 | in-progress | 用户确认触摸恢复，5103 条真实事件及 Qt 打开 Goodix/health=0 有记录；亮度新交互舒适度仍待反馈。临时系统层冷启动 WiFi 自动 DHCP、亮度服务 RPC 就绪及 Web 转发通过；合成点击只能证明 UI 路径，不能替代人工触摸精度。正式持久网络方案、工业从站互操作和完整现场验收待完成 |
| H04 | in-progress | 实板完整签名包安装、健康原码 73/失败回退、prepared 与 switched 两处物理断电恢复、原生亮度/锁/Web 转发及真实 Qt→GitHub 检查任务通过；序号 6 正常冷启动。fixture 契约和临时启动层不构成正式基线。正式硬件修订、信任根、维护互锁、GitHub 下载安装、完整掉电矩阵及数据迁移未验收 |
| H05 | not-run | 本轮稳定性/老化和多板量产证据缺失，仍为开发草稿；代码推送不等于正式 Release 或实板完整 OTA 通过 |

## 执行记录

Ralph 将在每轮记录：需求 ID、改动、验证命令与退出码、证据路径、残留问题。
本机通过项与硬件阻塞项分开描述，但不能把硬件项删除以宣布全部完成。


### 第 1 轮：S04 Qt 异常快照显示（2026-10-13）

本轮仅完成一个原子修复，保留全部原有未提交工作；未提交、推送、上传、发布或烧录。

- 修复总览设备数、告警数和运行时长缺测时默认为 0；现在显示 `--`，真实零值仍显示 0。
- 系统 CPU/内存/温度/存储标签先检查 JSON 数值类型和有限性；不再将非数值隐式转换为 0。
- 温度/压力状态条与曲线统一精确名称和单位绑定：温度仅接受 °C，压力接受 MPa/kPa 并转换为 MPa；拒绝将“温度设定值”当实测温度、将 °F 当 °C。
- 传感器明确离线、不可用或读数不是有限数时清除读数/曲线；未知或异常状态不再染成正常绿色。
- 无效或越界时间戳触发离线清理，包括监控暂停时，防止保留失效快照。
- 新增 3 个 Qt 回归场景，覆盖缺失/null/字符串/布尔/负数/极大统计值、非有限读数、错单位、错名称、数组换序、离线，以及暂停时无效时间戳；现有时间窗口、暂停、告警及其他页面回归继续通过。

| 验证命令 | 退出码与结果 | 证据 |
| --- | --- | --- |
| `make test` | 0；5 + 59 个 Python 测试及仓库门禁通过，S4 仍 unqualified | `dist/iteration-s04/make-test.log` |
| `make -j4 test-hmi` | 0；15 passed，0 failed，0 skipped（包含初始化/清理） | `dist/iteration-s04/test-hmi.log` |
| 在 `dist/ui-arm` 用 dev3 qmake 生成后 `make -j4` | 0；实际复用 dev2 的 Qt 5.11.3 / 目标 sysroot | `dist/iteration-s04/s4-build.log` |
| `readelf -h -A -d dist/ui-arm/omnigate-hmi` | 0；ARMv7 ELF32 EABI5 soft-float，记录 NEEDED 库；未在实板运行 | `dist/iteration-s04/arm-elf.log` |
| `git diff --check` | 0 | `dist/iteration-s04/diff-check.log` |

本轮从实际 Qt 测试留存五尺寸 × 14 个界面共 70 张截图，另有离线及异常传感器截图，
均位于 `dist/iteration-s04/screenshots/`。已人工查看 `1024x600-invalid-sensor.png`，
确认读数为 `--`、状态为“未获取”、趋势为空。其余截图的完整参考图视觉对照仍属 S02 待办；
这些截图使用测试夹具，不构成硬件证据。源码、字体/资源、ARM、截图与日志哈希写入本轮独立
`dist/iteration-s04/validation.json`，历史索引不冒充本轮证据。

S04 尚存独立软件问题：`components/web/hmi_api.py` 的 `_memory_percent` 在缺少 MemTotal 时返回 0，
`_cpu_percent` 首次采样使用 loadavg×25，短字段返回 0、空 stat 会索引失败，uptime 读不到时默认 0。
后续 runtime task `spec:s04-system-sampling` 已记录修复与回归要求，故 S04 保持 in_progress。

H01—H05 继续 blocked_external，具体缺失条件见上表。主机磁盘当时可用约 44 GiB，
本轮没有尝试完整 SDK/rootfs 镜像构建，不能以 ELF 构建替代 H02。
GUI 安装按钮继续禁用，安装需系统维护流程；尚无真实公钥、维护互锁、掉电恢复及回退实板证据。
本轮不发送软件全通过、实板全通过或全部完成事件；仍存在可推进的软件任务。


### 第 2 轮：S04 后端系统指标缺测与 CPU 首采（2026-10-13）

本轮仅完成 `spec:s04-system-sampling` 原子任务，保留全部进入本轮前的修改。

- CPU 首次读取只建立基线并返回 `null`；后续用累计计数差分计算，不再使用 loadavg×25。
  空/短/非数值/负数输入返回缺测并清除基线；计数回退、重复或字段数变化的区间返回缺测，后续有效区间可恢复。
  读取和基线更新在同一锁内完成，guest/guest_nice 不重复计入总时间。
- 内存要求有效的 MemTotal/MemAvailable 和 kB 单位；缺字段、零总量、负数或 available 超过 total 返回 `null`。
  不再用 MemFree 冒充 MemAvailable；未提供 MemAvailable 的内核显示缺测。
- 运行时长拒绝空值、非数值、非有限值和负数；存储读取失败或容量无效返回缺测。
  温度不可用时快照及遥测保持 `null`，真实零读数仍保留零。
- 6 个新增 Python 测试覆盖采样失败后恢复、CPU 计数异常、真实零值和 API→SQLite→历史接口的缺测传递。
  Qt 现有异常快照测试追加后端显式 JSON null 的 CPU/内存/温度/存储/运行时长显示检查。
- 本机临时 HTTP 服务通过实际 loopback socket 验证缺测快照、四类历史指标与恢复后的实测差分。
  该验证注入主机夹具，不是板端或工业设备证据。

| 验证命令 | 退出码与结果 | 证据（均在 `dist/iteration-s04-backend/`） |
| --- | --- | --- |
| `make test` | 0；5 + 65 个 Python 测试及仓库门禁通过，S4 仍 unqualified | `make-test.log` |
| `make -j4 test-hmi` | 0；15 passed，0 failed，0 skipped | `test-hmi.log` |
| `.test-venv/bin/python dist/iteration-s04-backend/http-smoke.py` | 0；实际本机 HTTP 缺测/恢复检查通过 | `http-smoke.log`、`http-*-snapshot.json` |
| 在本轮独立 `arm-build/` 用 dev3 qmake 后 `make -j4` | 0；从当前源文件重新编译 HMI，实际使用 dev2 sysroot | `s4-build.log`、`arm-build/omnigate-hmi` |
| `readelf -h -A -d` | 0；ARMv7 ELF32 EABI5 soft-float；依赖列表留存，未在板上运行 | `arm-elf.log` |
| `git diff --check` | 0 | `diff-check.log` |

源码、资源、ARM、测试日志、HTTP 验证脚本和截图哈希见本轮 `validation.json`。
本轮 Qt 截图另存 `screenshots/`，覆盖五尺寸 × 14 界面及两个异常场景。
没有进行全页面参考图人工复核，不将截图生成或测试夹具当成 S02/S03 视觉验收通过。

复核时发现另一个 S04 缺陷：`_temperature` 按数值是否大于 1000 猜测单位，
因此 sysfs 毫摄氏度 500、-1000、1000 返回 None，而应为 0.5、-1、1°C。
复现记录为 `thermal-followup.log`，独立 runtime task `spec:s04-thermal-units` 留待下一轮。
本轮 CPU/缺测任务已验证，但 S04 整项继续 in_progress。

H01—H05 缺失条件不变；本轮没有完整系统镜像、实板、签名、物理掉电或回退证据。
GUI 安装仍禁用，应用安装仍需系统维护流程。本轮未提交、推送、上传、发布或烧录。
仍有可推进的软件工作，不发送全部软件通过、实板通过、全部完成或外部全阻塞事件。


### 第 3 轮：S04 sysfs 温度单位与低温缺测（2026-10-13）

本轮仅完成 `spec:s04-thermal-units` 原子任务，保留全部已有修改。

- 按 [Linux thermal ABI](https://raw.githubusercontent.com/torvalds/linux/master/Documentation/ABI/testing/sysfs-class-thermal)
  将 `thermal_zone*/temp` 的整数毫摄氏度固定除以 1000；不再根据读数大小猜测单位。
  `500`、`1000`、`-1000` 分别返回 `0.5`、`1.0`、`-1.0°C`，真实零值仍是零。
- 保留原有 `-20 < °C < 150` 取值范围与一位小数输出，先比较整数范围再转换，超大输入不会造成浮点溢出。
  缺文件、读取失败、空值、非法/非有限文本与越界输入返回缺测；坏 zone 不阻止后续有效 zone。
- 新增 3 个 Python 行为回归，使用临时文件走真实读取/解析，验证失败后恢复及 API→SQLite→历史接口。
  本机 loopback HTTP 另行覆盖文件缺失、低温、1000 边界、NaN、负温、零及正常恢复，不替换温度采样函数。

| 验证命令 | 退出码与结果 | 证据（均在 `dist/iteration-s04-thermal/`） |
| --- | --- | --- |
| `make test` | 0；5 + 68 = 73 个 Python 测试及仓库门禁通过，S4 仍 unqualified | `make-test.log` |
| `make -j4 test-hmi` | 0；15 passed，0 failed，0 skipped | `test-hmi.log` |
| `.test-venv/bin/python dist/iteration-s04-thermal/http-smoke.py` | 0；真实本机 HTTP 温度/缺测/恢复与历史持久化通过 | `http-smoke.log`、`http-*-snapshot.json`、`http-history.json` |
| 独立 `arm-build/` 中 dev3 qmake 后 `make -j4` | 0；全量重编译，实际复用 dev2 sysroot | `s4-build.log`、`arm-build/omnigate-hmi` |
| `readelf -h -A -d` | 0；ARMv7 ELF32 EABI5 soft-float，动态依赖留存 | `arm-elf.log` |
| `git diff --check` | 0 | `diff-check.log` |

本轮 70 张五尺寸 × 14 界面截图和 2 张异常场景截图独立保存在 `screenshots/`。
本轮没有 UI 布局修改或完整参考视觉复核；这些是 Qt 测试夹具截图，不是实板证据。
源码、资源、ARM、验证脚本、日志和截图哈希见 `validation.json`。

复核实际 HTTP 缺测快照时发现，温度 unavailable、传感器数组为空、现场链路离线时，
`summary.status` 仍为“正常”，因为后端只检查活动告警数量。
证据见 `summary-followup.json`；后续任务 `spec:s04-summary-truth` 处理无活动告警与系统健康的语义混用。
本轮温度原子修复通过，S04 仍 in_progress，不把该缺陷隐藏为全项通过。

H01—H05 外部条件不变，无完整系统镜像、实板、正式签名、掉电或回退新增证据。
GUI 安装仍禁用。未提交、推送、上传、发布、烧录或格式化；软件仍可继续推进。


### 第 4 轮：S04 总览健康语义（2026-10-13）

本轮仅完成 `spec:s04-summary-truth` 原子修复，保留全部已有未提交修改。

- `summary.status` 不再把无活动告警当成系统正常。活动告警存在时保留“有告警”；
  无告警时显示“未验证”，演示模式无告警显示“演示”。真实告警在演示模式下仍优先显示。
- 当前未建立完整现场健康判定契约：真实传感器接入为空，RS485/EtherCAT 从站探测不可用，
  主机指标完整或接口 UP 也不能证明工业系统整体健康。因此不新造“健康”聚合判据。
  各指标、链路和服务继续单独呈现已有状态，告警确认不被当作故障恢复。
- 新增两个 Python 回归，覆盖 unknown/离线/在线链路、无设备配置、告警确认、演示切换及真实告警优先。
  既有缺测/恢复回归追加总览状态断言。新增 Qt 回归验证暂停时状态仍更新、非健康颜色及离线清理。
- 实际 loopback HTTP 覆盖缺测→真实告警→确认→演示告警→演示→关闭演示→主机指标恢复；
  保存七份真实 API 响应，并通过 Qt 网络回复处理入口逐份回放，检查总览与底栏一致。
  HTTP 服务与设备输入均为主机测试夹具，不是实板证据。

| 验证命令 | 退出码与结果 | 证据（均在 `dist/iteration-s04-summary/`） |
| --- | --- | --- |
| `make test` | 0；5 + 70 = 75 个 Python 测试及仓库门禁通过，S4 仍 unqualified | `make-test.log` |
| `.test-venv/bin/python dist/iteration-s04-summary/http-smoke.py` | 0；七个 HTTP 状态转换通过 | `http-smoke.log`、`http-*-snapshot.json`、`http-snapshots.json` |
| `HMI_SUMMARY_HTTP_REPLAY=$PWD/dist/iteration-s04-summary/http-snapshots.json make -j4 test-hmi` | 0；16 passed，0 failed，0 skipped，含 HTTP 快照回放 | `test-hmi.log` |
| 独立 `arm-build/` 中 dev3 qmake 后 `make -j4` | 0；当前 HMI 全量重编译，实际仍复用 dev2 sysroot | `s4-build.log`、`arm-build/omnigate-hmi` |
| `readelf -h -A -d` | 0；ARMv7 ELF32 EABI5 soft-float，动态依赖留存 | `arm-elf.log` |
| `git diff --check` | 0 | `diff-check.log` |

本轮 Qt 截图保存于 `screenshots/`，含五尺寸 × 14 个独立界面的 70 张标准截图、
20 张主导航重复视图和 3 张异常状态图，共 93 张。人工查看了
`1024x600-summary-unverified.png`，确认总览和底栏均显示黄色“未验证”、缺测读数为 `--`。
未完成全页面参考图视觉对照，不将截图生成作为 S02/S03 通过证据。
源码、资源、ARM、日志和截图哈希见 `validation.json`。

S04 已知 summary 缺陷已修复，但整项仍需最后复核精确时间窗口边界、时钟倒退及暂停隔离；
现有时间序列测试覆盖重复时间戳和 +61 秒，未把未覆盖边界标成通过。
后续 runtime task `spec:s04-window-acceptance` 已登记。其他软件项仍可推进。
H01—H05 条件不变；无完整系统镜像、实板、正式签名、物理掉电或回退证据。
GUI 安装仍禁用。未提交、推送、上传、发布、烧录或格式化，不发出全通过或外部全阻塞事件。


### 第 6 轮：S05 CAN/NMT/SDO 参数与能力门禁（2026-10-13）

本轮只实施 `spec:s05-can-controls`，保留所有既有修改，不提交、推送或发布。

- CAN 仲裁波特率在 Qt 与 API 校验 10000..1000000；CAN-FD 数据波特率须不低于仲裁波特率且不超过 8000000。经典 CAN 忽略禁用的数据速率字段，配置明确传递 `fd off`，接口 down 失败立即停止后续配置。
- CAN 通道必须具有非空设备名；CAN-FD、CANopen 分别要求板型声明对应能力。未声明、API 离线及恢复均有回归。经典 CAN 配置允许已映射但尚未 UP 的接口，以便配置/启用；能力声明和主机测试不证明现场链路可用。
- 危险操作仍默认“否”；确认期间重新检查最新快照，掉线、接口/能力撤销后不发送请求。NMT 确认显示接口与节点，节点 0 明确为广播。
- SDO 节点 1..127、索引 0..65535、子索引 0..255，Qt 表单写入固定 4 字节，接受 -2147483648..4294967295。API 对 1..4 字节分别校验可表示范围，非法输入在连接前拒绝。
- 修复原实现未加载 EDS 却访问对象字典、忽略 size 的问题。改用 `sdo.download(index, subindex, payload)`，明确 little-endian 和负数补码。依据 [CANopen 官方 SDO API](https://canopen.readthedocs.io/en/stable/sdo.html)，无对象字典写入可使用 download；SDK python-canopen.mk 记录版本为 2.4.1，本轮使用相同库版本作虚拟总线验证。
- HTTP 确认只接受 JSON `true`，字符串/数字不视为确认。SDO abort 保留失败审计、断开连接，下一次正常写入可恢复。

所有路径以下均相对 `dist/iteration-s05-can/`：

| 验证 | 结果 | 证据 |
| --- | --- | --- |
| `make test` | 0；5 + 75 = 80 个 Python 测试及仓库门禁通过，含原 T153/更新器回归，S4 仍 unqualified | `make-test.log` |
| `make -j4 test-hmi` | 0；21 passed，0 failed，0 skipped；五尺寸全部页面、确认/撤权/请求内容回归 | `test-hmi.log` |
| Qt CAN 定向测试，`HMI_CAN_REQUESTS_OUT=...` | 0；捕获 4 个真实 Qt HTTP POST，含经典 CAN、FD、NMT 广播和 SDO | `can-qt.log`、`qt-requests.json` |
| `.test-venv/bin/python dist/iteration-s05-can/http-can.py` | 0；经实际 loopback HTTP 重放 Qt 请求，真实 CANopen 2.4.1 + python-can 4.3.1 虚拟帧、SDO 确认/abort/恢复、NMT 广播/定向和 SQLite 审计通过 | `http-can.log`、`http-can-evidence.json`、`python-deps.log` |
| 独立 arm-build 中 S4 dev3 qmake + `make -j4` | 0；实际复用 dev2 sysroot，未重建系统镜像 | `s4-build.log`、`arm-build/omnigate-hmi` |
| `readelf -h -A -d`、`git diff --check` | 0；ARMv7 ELF32 EABI5 soft-float，动态依赖留存；空白检查通过 | `arm-elf.log`、`diff-check.log` |

实际截图共 93 张（70 张五尺寸独立界面、20 张主导航重复视图、3 张异常图）保存 `screenshots/`。
人工查看本轮 1024×600 CAN/NMT 与 SDO 截图，表单和操作可见；未完成全页面参考图对照，不将 S02/S03 改为 passed。
HTTP/虚拟 CAN 测试只替代物理传输层和 ip 命令，不向真实设备发出控制；不是实板互操作证据。
首次新增测试失败日志保留 `make-test-before.log`、`test-hmi-before.log`：分别为错误响应结构断言错误和 QMessageBox 关闭方法未模拟标准按钮点击，已修正测试后重新通过，没有跳过失败场景。

恢复检查发现第 5 轮未产出其宣称的 validation.json，且 scratchpad/执行记录/任务关闭未完成；已将 S04 恢复为 in_progress，保留原日志与实现，交回原任务复核归档。本轮没有再次实施 S04。
S05 RS485/EtherCAT 另列后续任务；H01—H05 条件不变。GUI 安装仍禁用，全部成果仅本地保存。


### 第 7 轮：S04 时间窗口/暂停验收恢复归档（2026-10-13）

本轮只完成 reopened 的 `spec:s04-window-acceptance`。保留第 5 轮原目录与全部未提交工作，
不补造历史索引；在独立 `dist/iteration-s04-window-recovery/` 重新验证当前源码。
现有实现满足本原子任务，没有发现需额外修改的产品逻辑，不重写已有修复或重复新增测试。

- 两个趋势组件验证最近 60 秒闭区间：59.999/60/60.001 秒边界、同时间点替换、时钟倒退清空、
  大步前进过期清理和断线恢复；使用真实时间坐标绘图。
- 复核第 5 轮修复：恢复监控显示先清空该页趋势，再应用最新快照，避免连接暂停期间缺测或时钟回退的两段数据。
  暂停只冻结监控读数/趋势；总览、系统指标、网络、服务及告警仍由新快照更新。
- 实际 Qt 网络轮询连接本机 Flask：6 次 GET、6 条 SQLite 遥测（41..46°C）及 1 条活动告警；
  暂停期间未发送 POST 或控制命令。第 5 次注入无效时间戳清空旧显示，第 6 次恢复数据，解除暂停后恢复监控。
  温度/设备数据为明确主机夹具和演示传感器，不能据此声称真实工业采集或实板互操作通过。
- 当前全套回归同时覆盖前轮有限数/单位/离线、CPU 首采与恢复、sysfs 低温及总览未验证语义。
  S04 本机软件行为现有证据齐全，改为 passed；S02/S03 完整视觉验收仍未通过。

以下路径均相对 `dist/iteration-s04-window-recovery/`：

| 验证命令 | 退出码与结果 | 证据 |
| --- | --- | --- |
| `make test` | 0；5 + 75 = 80 个 Python 测试及仓库门禁通过，含原 T153/更新器回归；S4 仍 unqualified | `make-test.log` |
| `make -j4 test-hmi` | 0；21 passed，0 failed，0 skipped，含窗口边界与暂停失败/恢复 | `test-hmi.log` |
| `.test-venv/bin/python dist/iteration-s04-window-recovery/http-pause.py` | 0；Qt 定向 3 passed（含初始化/清理），真实 HTTP 轮询与 SQLite 检查通过 | `http-pause.log`、`live-qt.log`、`http-pause-evidence.json` |
| 独立 `arm-build/` 中 S4 dev3 qmake + `make -j4` | 0；当前 HMI 全量编译，实际复用 dev2 sysroot，非完整系统镜像 | `s4-build.log`、`arm-build/omnigate-hmi` |
| `readelf -h -A -d` | 0；ARMv7 ELF32 EABI5 soft-float，动态依赖留存 | `arm-elf.log` |
| `git diff --check` | 0 | `diff-check.log` |

本轮 93 张实际 Qt 夹具截图包含五尺寸 × 14 个独立界面、20 张重复主导航视图及 3 张异常图，
保存于 `screenshots/`。查看了 1024×600 设备监控图：横轴 -60..0 秒、读数及暂停按钮可见；
未作全页面参考图对照，不把截图生成当作 S02/S03 或实板验收。当前源码/资源、主机测试 ELF、
ARM ELF、日志、HTTP 脚本/证据与截图哈希统一记录 `validation.json`，并重新校验索引列出的全部文件。

S05 RS485/EtherCAT 仍有 ready task，其他软件需求仍可推进；H01—H05 外部条件不变。
GUI 安装继续禁用，安装仍需系统维护流程。未提交、推送、上传、发布、烧录、格式化或联网发消息。
本轮仅关闭经验证的原子任务，不发送全部软件通过、实板通过、全部完成或外部全阻塞事件。


### 第 8 轮：S05 RS485/EtherCAT 参数与实际请求（2026-10-13）

本轮仅完成已有 RS485/EtherCAT 原子任务；全部既有未提交成果保留，没有提交或远端写入。

- Qt 与 HMI API 同时要求 `modbus-rtu`/`ethercat` 能力及非空、类型/角色正确的板级设备映射。串口只能使用映射通道，不再接受任意匹配名称的 `/dev/ttyAS*`；EtherCAT 主站必须唯一。
- RS485 校验波特率 300..4000000、站号 1..247、轮询周期 50..3600000；EtherCAT 索引 0..65535、子索引 0..255、1..32 字节十六进制。后台周期参数还拒绝设定等待总时长超过 10 秒的请求，避免接受必然超过 20 秒命令超时的组合；实际现场耗时仍可能超时并如实失败。
- 确认默认拒绝；确认期间断连、能力撤销或主站映射变化不发送请求。EtherCAT 请求绑定确认时的接口，服务端重新比对映射。
- 串口按钮改为“保存配置”，明确仅修改首个从站、运行生效状态未验证。API 只写配置，不重启采集服务；其余从站、帧格式与采集点保留。没有添加未经契约支持的实时生效/寄存器读取功能。
- 寄存器/从站表保持真实空态，周期与抖动仍为未测量。周期确认不再保证实际耗时恰为 1 秒。

验证命令及结果：

- `make test`：exit 0；5+78=83 个 Python 测试及仓库门禁，覆盖 T153 既有映射、S4 未声明能力拒绝、参数边界、非法请求无副作用及配置写入失败恢复。日志 `dist/iteration-s05-serial-ethercat/make-test.log`。
- `HMI_SERIAL_ETHERCAT_REQUESTS_OUT=... make -j4 test-hmi`：exit 0；23 passed、0 failed、0 skipped。实际 Qt 网络捕获 3 个已确认 POST；非法/取消/确认中撤权无请求，映射恢复可用，刷新保留表单编辑。日志 `test-hmi.log` 与 `qt-requests.json`。
- `.test-venv/bin/python dist/iteration-s05-serial-ethercat/http-controls.py`：exit 0；18 次实际 loopback HTTP 请求，回放 Qt 请求，验证真实临时配置文件与子进程执行边界；未确认、错映射、非法数据、磁盘失败、子进程失败及超时均正确拒绝/记录，随后恢复成功。日志 `http-controls.log`、`http-evidence.json`。子进程为明确的主机夹具，不是 SOEM/实板传输证据。
- 独立 `arm-build/` 中目标 qmake + `make -j4`：exit 0；dev3 qmake 实际复用 dev2 sysroot；ELF32 ARMv7 EABI5 soft-float。日志 `arm-build.log`、`arm-elf.log`。
- `git diff --check`：exit 0；源码、字体/资源、测试二进制、ARM ELF、日志和截图哈希见 `validation.json`。

五尺寸 70 个必需视图（含重复主导航和异常图共 97 张截图）保存于本轮 `screenshots/`。人工查看 1024×600 RS485/EtherCAT 常规、缺能力空态与离线图；未进行全页面参考视觉比对，S02/S03 不改为通过。S05 仅 HMI 软件行为验收通过；H01—H05 仍缺真实外部证据，其他 S 项可继续推进，不发送全部通过或外部全阻塞事件。


### 第 9 轮：S06 网络分类、选择与蜂窝能力门禁（2026-10-13）

本轮仅完成现有 S06 任务，保留所有进入本轮前的修改，不提交或远端写入。

- 有线/无线仅按板级通道归类，取消 eth/en/wl 名称猜测；蜂窝配置中的网络接口明确标记 cellular，不能混入有线页。未知映射不冒充已支持接口。
- 刷新按接口名称保留选择；所选接口消失后，即使其他行仍存在也清空详情，连续刷新不会自动选另一个设备。用户重新选择后恢复保持；服务离线清空三个子页的旧地址和详情。
- 蜂窝快照仅暴露已验证的 profile→设备/接口映射，不暴露 APN 账号、密码或会话句柄。按钮要求 dual-cellular 能力、有效板级 cellular 通道及对应配置；未配置的另一模块保持禁用。
- 确认默认拒绝；确认期间断线、撤权或映射变化不发送请求。发送时携带确认过的设备/接口，后台再次比对。后端拒绝缺能力、错设备、非法/冲突/重复网络映射及陈旧请求，保留 T153 已有双蜂窝映射，S4 默认无能力时拒绝。
- 断开要求匹配设备/接口的有效会话句柄，QMI 或接口命令失败均如实失败；拨号缺会话句柄、DHCP 失败或超时不报成功。DHCP 失败保留已建立会话的句柄供后续断开，未宣称自动恢复/回滚拨号。
- 蜂窝页展示实际接口与地址；链路 UP 明确为“链路已连接 / 拨号状态未验证”，不据此推断端到端网络可用。信号、DHCP 配置和未获取的地址保持未知；删除了快照中对 wwan0/1 的虚构在线/备用状态。

验证及证据（路径相对 `dist/iteration-s06-network/`）：

| 验证 | 结果 | 证据 |
| --- | --- | --- |
| `make test` | exit 0；5+81=86 Python 测试及仓库门禁，包括 T153、更新器已有回归 | `make-test.log` |
| `HMI_CELLULAR_REQUESTS_OUT=... make -j4 test-hmi` | exit 0；25 passed、0 failed、0 skipped；真实 Qt 捕获两个确认 POST，拒绝/取消路径无请求 | `test-hmi.log`、`qt-requests.json` |
| `.test-venv/bin/python dist/iteration-s06-network/http-controls.py` | exit 0；19 次实际 loopback HTTP，Qt 请求回放、配置/能力拒绝、QMI/DHCP 失败、超时、审计与恢复 | `http-controls.log`、`http-evidence.json` |
| 独立 `arm-build/` 中 S4 dev3 qmake + `make -j4` | exit 0；实际复用 dev2 sysroot；ARMv7 ELF32 EABI5 soft-float | `arm-build.log`、`arm-elf.log` |
| `git diff --check`、哈希归档校验 | exit 0 | `diff-check.log`、`validation.json` |

五尺寸 70 个必需页面视图及异常图共 101 张真实 Qt 夹具截图在 `screenshots/`；查看了 1024×600 三个网络页、已映射蜂窝及三个离线页。未完成全页面参考视觉验收，S02/S03 保持未通过；人工查看发现底栏离线文字正确但圆点仍呈绿色，留给 S02 的状态色视觉修复。测试使用主机传输夹具，不能证明 QMI 硬件、DHCP 网络或实板互操作。

S06 仅本机软件行为通过。S07 及其他软件需求仍可推进，H01—H05 的外部缺项不变；GUI 安装保持禁用。没有完整系统镜像、实板或正式更新闭环新证据，不发送全部软件通过、实板通过、全部完成或外部全阻塞事件。


### 第 10 轮：S07 告警、确认、导出与操作日志（2026-10-13）

本轮只完成现有 S07 原子任务，保留全部既有修改；明确禁止提交优先于通用工作流模板，未提交、推送、上传或发布。

- 告警 ID 不再经 32 位整数转换，实际 Qt/HTTP 验证 ID `2147483655`；非整数、缺失及超出 JSON 精确整数范围的 ID 禁止确认。详情明确显示对应 ID/时间和纯文本消息，不把 HTML 当富文本。
- 级别、来源、最近 24 小时/7 天和确认状态使用原始字段过滤；缺失/非法/未来时间不进入“最近”范围。刷新重排按 ID 保持选择，所选行隐藏/消失或主动取消选择后清空详情并禁用确认。已选来源消失仍保留该条件和空态，不擅自切回全部。
- 仅允许确认可见、选中、活动且 ID 有效的记录；快照离线、告警获取失败或正在确认时禁用。离线期间返回告警列表仍不能提前启用确认。实际 Qt 捕获验证隐藏行、离线、重复点击无额外 POST，刷新恢复后发送原 ID；错误不弹成功提示。
- 后端拒绝缺失 ID、非活动状态和非演示模式下不可见的演示告警；重复确认幂等，不覆盖首次时间/操作者，也不重复写审计。确认与 `alarm_ack` 审计在同一个 SQLite 写事务中提交，注入审计存储失败时状态回滚，恢复后重试成功。未要求现场服务停止或硬件控制。
- 导出固定按钮点击时的可见记录；文件对话框期间刷新不改变导出集合。保持 UTF-8 BOM、CSV 引号/逗号/CRLF 转义和公式前缀防护；纯空白内容不无故加前缀。实际文件对话框与 QSaveFile 验证成功、取消、无法写入，独立 Python csv 解析验证完整字段和隐藏记录排除。
- 日志成功/失败过滤只匹配原始 `success`/`failed`；未知状态保留原文和中性色，不能误算为失败。`alarm_ack` 显示“告警确认”，原始动作/详情保留在提示中。

验证和证据（相对 `dist/iteration-s07-alarms-audit/`）：

| 验证 | 结果 | 证据 |
| --- | --- | --- |
| `make test` | exit 0；5+84=89 Python 测试及仓库门禁，包含现有 T153/更新器回归 | `make-test.log` |
| `HMI_ALARM_CSV_OUT=... HMI_ALARM_REQUESTS_OUT=... make -j4 test-hmi` | exit 0；28 passed、0 failed、0 skipped | `test-hmi.log`、`qt-requests.json`、`visible.csv` |
| `.test-venv/bin/python dist/iteration-s07-alarms-audit/http-controls.py` | exit 0；18 次真实本机 HTTP；Qt→Flask→SQLite→Qt 确认/刷新/审计，事务失败恢复及独立 CSV 解析 | `http-controls.log`、`http-evidence.json`、`qt-live-http.log` |
| 独立 `arm-build/` 目标 qmake + `make -j4` | exit 0；dev3 qmake 实际复用 dev2 sysroot；ARMv7 ELF32 EABI5 soft-float | `commands.txt`、`arm-build.log`、`arm-elf.log` |
| `git diff --check` 与源码/资源/产物哈希校验 | exit 0 | `diff-check.log`、`validation.json` |

五尺寸 70 个必需界面视图及重复/异常图共 105 张截图位于 `screenshots/`；人工查看告警选中详情、空态、离线和日志未知结果的 1024×600 实际 Qt 图。全页面参考视觉复核未完成，S02/S03 不改为通过；底栏离线圆点颜色仍为已记录的 S02 待修项。快照采样为明确主机夹具，截图中的示例运行值不是实板数据。

告警筛选与导出限已加载最近 500 条，操作日志限最近 200 条；确认仅表示操作员已确认，不能推断故障恢复。GUI 更新安装仍保持禁用。本轮 S07 仅本机软件行为通过；S08 及其他软件项可继续推进，H01—H05 无新增外部证据，不发送软件全通过、实板通过、全部完成或外部全阻塞事件。


### 第 11 轮：S08 显示、服务与更新检查（2026-10-13）

本轮仅处理既有 S08 任务，保留全部先前未提交工作。明确禁止提交、推送、上传、发布、烧录和删除既有成果的要求优先于通用任务模板；没有进行远端写入或实板操作。

- 设置编辑从首次快照起保护；保存时固定确认的数据和编辑序号，响应不清除发送后新增的编辑。保存过程中禁用重复提交，确认期间离线不发送。保留后端既有轮播设置；1.5 秒等有效非预设刷新周期按实际值显示。
- PUT 设置与 POST 背光保持分开的结果：只有响应与提交值一致才提示完成，背光失败明确“设置已保存，背光应用未确认”。后端拒绝非布尔演示模式和非法范围；保存失败保持原配置。真实背光硬件没有接入，本轮以临时文件验证调用和持久化，不声称物理亮度变化。
- 服务按钮需快照明确提供 available=true，离线/缺服务时禁用，确认后再次核验。后端检查可执行服务和蓝牙板级能力，非零退出、缺命令、超时以及 NTP 打印 FAIL 都失败并记录失败审计。命令成功只说明执行结果，服务是否运行以设备状态接口为准。
- 仓库 ThingsBoard 脚本有 0/1 运行状态契约；蓝牙 status 仅是诊断，S49ntp 没有 status 操作，因此后二者保持未知，不能把命令返回码或可执行文件存在当成运行状态。服务采样超时也返回未知，不令整份快照失败。
- 更新页检查系统记录中的当前应用版本/板型/契约是否非空，每次检查重新读取身份；由系统更新器校验签名/兼容性，前端验证其候选结果形状。覆盖兼容候选、无候选、字段缺失、非法 JSON/版本、非零退出、启动失败和身份缺失。失败不残留绿色通过或“已找到”说明；外来版本文本按纯文本处理。清单已验证不表示应用包哈希已验或已安装。
- GUI 安装按钮仍禁用，正式安装仍须系统维护流程；没有创建生产签名材料或执行远端检查。QProcess 测试使用临时测试命令；S09 协议复核和 H04 正式更新闭环不以此替代。

验证命令及结果（全部最终退出码 0）：

| 检查 | 证据 |
| --- | --- |
| `make test`：core 5 + 其余 87 = 92 个 Python 测试及仓库门禁 | `dist/iteration-s08-settings-services-updates/make-test.log` |
| `HMI_SETTINGS_REQUESTS_OUT=… make -j4 test-hmi`：30 passed，0 failed，0 skipped | 同目录 `test-hmi.log`、`qt-requests.json` |
| `.test-venv/bin/python dist/iteration-s08-settings-services-updates/http-controls.py`：19 次真实 loopback HTTP，Qt→Flask 设置/背光、服务子进程失败恢复、9 条真实 SQLite 审计 | 同目录 `http-controls.log`、`http-evidence.json`、`qt-live-http.log` |
| dev3 qmake + 独立目录 `make -j4`、`readelf -h -A -d` | 同目录 `arm-build.log`、`arm-elf.log`；实际 dev2 sysroot，ARMv7 ELF32 EABI5 soft-float |
| `git diff --check`、源码/资源/产物哈希归档 | 同目录 `diff-check.log`、`validation.json`、`archive.py` |

最初 Python 新增背光测试过度 mock 了 settings 临时文件写入，已修正测试隔离并重跑通过；失败日志保留 `make-test-first.log`，未把失败记为通过。
当前截图共 115 张，包含五尺寸 × 14 个必需视图及失败/离线状态。已查看 1024×600 显示设置、服务可用/离线、离线编辑、更新成功/失败/缺身份实际 Qt 图；仅复核本轮行为可见性，没有完成 S02/S03 的全页面参考视觉验收。底栏离线圆点仍为绿色，保留给既有视觉任务。
S08 只标记本机软件行为通过；S01/S02/S03/S09/S10/S11/S12 及 H01—H05 仍按表中真实状态保留。下一独立协议复核任务已登记；没有发出软件全通过、实板通过、全部完成或全部外部阻塞事件。


### 第 13 轮：S02/S03 全页面视觉与小屏修正（2026-10-13）

本轮选择既有视觉任务 `task-1791878316-347c`。保留全部历史未提交成果；明确禁止提交/上传的要求优先于通用工作流。未提交、推送、联网发消息、发布或实板操作。第 12 轮 S09 没有最终事件及完整验收归档，本轮不补造其历史通过记录。

- CAN 去除左右表单内层滚动，保留页面级滚动；合并接口与 CAN-FD 行、去除 NMT/SDO 重复标题，让 44px 输入和操作在 1024×600 完整显示。800×480 滚动后操作按钮完整可见；600×1024 纵排。
- 底栏离线时显式切换红色，未知/未验证为黄色，恢复依最新状态；中心网关离线文字改中性色。前次正常绿色不会遗留到离线。
- 温度/压力图例分别使用曲线深蓝/浅蓝，图例13px、轴字增大，告警正文14px、时间/来源12px；没有改动真实采样、单位和时间窗口逻辑。
- 输入框与主操作保持至少44×44，表格行统一44px，数字输入的禁用态同步为灰色。通过调整分区间距、重复标题和列宽下限消除溢出，没有缩小字体或隐藏操作。
- 800×480 总览的横向摘要/拓扑原先文字相挤；可用宽度低于760时改为两列摘要和纵向拓扑，滚动查看趋势/告警。其他通用双栏仍按窗口宽度重排。
- 显示/交互页补齐监视器、亮度、字体、布局、语言、刷新和服务的线性行图标。更新主卡保留当前/可用版本和检查结果，硬件/契约标识在纯文本兼容性详情中可查；真实点击验证详情内容，GUI 安装继续禁用。
- 参考未接入的寄存器/从站/抖动、静态网络配置、信号和长时间窗继续明确为空态或只读，不虚构控件成功；服务/审计/蜂窝等无独立原图的子页沿用同一图标与组件体系。

| 验证 | 结果 / 证据（均在 `dist/iteration-s02-s03-visual/`） |
| --- | --- |
| `make test` | 0；core 5 + 93 = 98 个 Python 测试和仓库门禁，`make-test.log` |
| `make -j4 test-hmi` | 0；31 passed，0 failed，0 skipped，`test-hmi.log` |
| Qt 实际交互/几何 | 五尺寸所有主按钮可滚动至完整可见，SDO 打开确认并点“否”，总览告警入口实际跳转；300 个主控件量测、表格横向滚动/行高检查，`touch-measurements.json` |
| 独立 dev3 qmake + `make -j4` | 0；`arm-build.log`、最终样式增量 `arm-build-final.log`；实际使用 dev2 sysroot |
| `readelf -h -A -d` | ARMv7 ELF32 EABI5 soft-float，`arm-elf.log`；未在板上运行 |
| `git diff --check` / 哈希索引 | 0；`diff-check.log`、`validation.json`、可重验 `archive.py` |

本轮共304张 Qt PNG，包含五尺寸70个标准视图、70个滚动后操作位置、基准14页各自的空态/离线及行为失败/恢复图。逐页看过五尺寸构图并对照五张原图，复核了全部基准空态/离线，发现的问题已修复并重跑；最终禁用数字框样式追加查看 CAN/SDO 与 EtherCAT 图。`visual-review.json` 为14页具体结论和截图/原图哈希，`index.html` 可本地逐页切换参考、分辨率、空态和操作位置。`review-drafts/` 是过程图，不作为最终通过证据。

保留早期失败日志：44px 控件首次造成 CAN 高度超出，后续分区重排修复；新增测试第一次误点不存在的 Cancel（实际为 Yes/No）已更正，未改变确认逻辑。第五次布局回归已通过。首次全量 Python 检查发现第12轮遗留测试期待“草稿拒绝”，但先触发“发布门禁不完整”；仅修正合成主机夹具，分别断言两道拒绝且没有维护/停止钩子调用，更新器产品逻辑不变，未削弱门禁。

S02/S03 仅标记本机视觉和布局验收通过。S01/S09/S10/S11/S12 与 H01—H05 尚未全部完成；没有完整镜像、LinuxFB、实际触摸/软键盘/DPI、工业从站或正式更新闭环证据。S4 保持 unqualified/draft；不发送软件全通过、实板通过、全部完成或全部外部阻塞事件。


### 第 14 轮：S09 更新协议恢复与发布配对（2026-10-13）

保留第 12 轮已有代码和原目录，本轮重新验证当前源码，独立归档到 `dist/iteration-s09-recovery/`。

- 复核已有严格身份/版本/数据格式/防重放检查、显式渠道声明及布尔开发模式；签名公钥仍由系统指定。
  应用包边界、存储空间、锁、维护钩子、持久事务、健康检查、回退和用户数据保护测试均保留并通过。
- 发现公开发布证据虽匹配目标字段，却接受缺少/不允许的渠道和错误更新器版本。六个反例在
  `release-gate-red.log` 真实失败；抽出 `match_system_contract` 供设备匹配与离线发布门禁共用后回归通过。
  公开证据仍须匹配产品、仓库、板型/修订、ISA/float ABI、libc、runtime、contract 和数据格式。
- 新增 `tests/test_application_update_cli.py`：10 个测试共 38 次真实 CLI 子进程调用，执行实际 OpenSSL
  签名校验与临时系统钩子；覆盖全字段错配、同尺寸包篡改、签名破坏、已签名路径穿越、跨进程锁、
  维护拒绝后重试、健康失败回退、回退失败保留事务和后续恢复、已提交事务清理、防重放。
- 在 stop 钩子处（切换前）和 start 钩子处（切换后）分别向更新器发送 SIGKILL；新进程执行 recover 恢复旧版。
  每次 CLI 都检查用户数据、工厂预装身份和系统契约原字节未变；活动版本独立变更，已接受序号不回退。
  这是主机进程中断，不能替代真实掉电、SD NAND 持久性或板上服务健康验收。
- 临时 RSA 私钥仅由测试在系统临时目录创建并清理；合成 qualified 证据仅验证门禁，不作为正式发布资产。
  未执行 GitHub 更新查询或下载、未发布、未烧录、未格式化、未提交或上传。

| 验证命令 | 退出码与结果 | 证据（均在 `dist/iteration-s09-recovery/`） |
| --- | --- | --- |
| `UPDATE_CLI_EVIDENCE=... make test` | 0；5+104=109 Python、T153 回归与仓库门禁通过 | `make-test.log`、`cli-processes.jsonl` |
| `make -j4 test-hmi` | 0；31 passed、0 failed、0 skipped | `test-hmi.log`、`screenshots/` |
| dev3 qmake + 独立 `arm-build/` 中 `make -j4` | 0；实际使用 dev2 sysroot，未构建完整镜像 | `s4-build.log`、`arm-build/omnigate-hmi` |
| `readelf -h -A -d` | 0；ARMv7 ELF32 EABI5 soft-float | `arm-elf.log` |
| `git diff --check` 与本轮 `archive.py` | 0；当前源码/资源/ARM/日志/截图哈希逐项复核 | `diff-check.log`、`archive.log`、`validation.json` |

304 张当前 Qt 截图包含五尺寸 ×14 个标准界面。已查看 1024×600 应用更新页：身份缺失仍显示
未提供/未就绪，GUI 安装仍禁用。没有修改 UI，完整参考视觉沿用第 13 轮专项记录，不冒报本轮重新逐图验收。
`protocol-review.json` 记录 S09 各门禁到行为测试的映射及证据边界。早期未修复配对的通过日志和六个红例
均保留，并明确不是最终源码的通过证据。

S09 仅本机软件行为 passed。S01/S10/S11/S12 仍可推进，H01—H05 无新增外部证据；下一原子任务为
S10 目标运行依赖、字体许可/资源与打包复核。不发送软件全通过、实板通过、全部完成或全部外部阻塞事件。


### 第 15 轮：S10 目标依赖、字体资源与打包（2026-10-13）

本轮只完成已有 S10，保留全部既有工作。未提交、推送、上传、发布、联网发消息、烧录或格式化。

- 实际 Qt QRawFont 复现 Dshan Sans 缺失 U+00B7（·），直接字形检查同时发现 U+25A3（▣）；
  补齐 Regular/Bold 子集，当前 HMI 源码全部可打印字符均由内嵌字体支持。原字体保存在本轮
  `font-before/`；新字体使用原 TTC 哈希和 FontTools 4.5.0，在独立 `font-reproduced/` 重建后逐字节一致。
- 清单增加许可哈希并一同内嵌到 Qt 资源。Qt 检查实际内嵌字体的哈希、族名、完整源码字形覆盖、
  OFL 文本和上下箭头；检查新 ARM ELF 含本次生成的完整 RCC 数据。字体/图标与 Web 页面资源无需外网。
- `check_hmi_resources.py` 进入主机门禁与 S4/应用共享载荷构建；错误字体、许可、清单、缺失资源和
  非内嵌路径拒绝打包。S4 安装在删除旧预装目录前先验证，真实 CLI 反例验证失败仍保留旧预装记录。
- T153 保留原 BSP/能力/构建配方；接入脚本将 GPL LICENSE 放入实际 local-site 的 src 目录，
  安装规则同时携带 Dshan Sans OFL 和哈希清单。主机运行真实接入脚本及 make 安装规则并检查字节，
  使用明确占位原生程序，不能视为 T153 新交叉构建或实板证据。
- 当前 HMI 使用 S4 dev3 qmake 独立编译，实际仍为 dev2 sysroot；ELF32 ARMv7 EABI5、softfp 调用约定、
  `/lib/ld-linux.so.3`。递归检查 HMI、LinuxFB/输入/网络插件、Python/SSL/SQLite/zlib 等 52 个 ELF，
  核对架构、浮点 ABI、加载器、NEEDED 和符号版本。隔离树缺少 QtCore 或传入 x86 HMI 时拒绝，恢复后通过。
- 真实 ARM 文件通过 `install_s4_runtime.py` 安装到临时目标；与应用共享载荷逐字节相同，11 个文件
  压入本地未签名归档并经真实 `extract_payload` 解包后哈希一致。只含 bin/lib/share；不含系统库、
  设备身份、更新器、密钥或用户数据。系统契约保持未验证模板，未创建 UDISK 数据。
- 正式应用打包 CLI 因当前工作树未提交按预期拒绝，未创建签名材料或正式发布文件。
  本轮未签名归档只是隔离打包证据，不能用于安装或发布。

| 命令/验证 | 结果 | 证据（均在 `dist/iteration-s10-runtime-resources/`） |
| --- | --- | --- |
| `make test` | exit 0；5+107=112 Python 及 T153/仓库/字体门禁 | `make-test.log` |
| `make -j4 test-hmi` | exit 0；32 passed，0 failed，0 skipped；五尺寸70标准视图 | `test-hmi.log`、`screenshots/` |
| `python3 scripts/build_hmi_fonts.py --output dist/iteration-s10-runtime-resources/font-reproduced` | exit 0；字体、许可及清单全部逐字节一致 | `font-reproduce.log`、`font-reproducibility.json` |
| 独立 `arm-build/` 中 dev3 qmake + `make -j4` | exit 0；当前源码/字体，未构建系统镜像 | `s4-build.log`、`arm-elf.log` |
| `python3 .../audit-runtime.py --root <dev2 target> --hmi .../arm-build/omnigate-hmi --output .../runtime-dependencies.json` | exit 0；52 ELF 静态依赖与符号版本 | `runtime-audit.log`、`runtime-dependencies.json` |
| `python3 .../runtime-failures.py` | exit 0；缺库/错 ISA 拒绝、恢复通过 | `runtime-failures.json`、对应日志 |
| `python3 .../stage-payload.py` | exit 0；预装/共享载荷/解包一致，脏源码正式打包拒绝 | `packaging.log`、`packaging.json`、`dirty-package-rejection.log` |
| `python3 .../python-runtime-smoke.py` | exit 0；系统宿主 Python 3.8 加载目标固定9项纯 Python 依赖，当前 S4 health/snapshot API通过 | `python-runtime.log`、`python-runtime.json` |
| SDK 宿主 Python 3.7 源码编译检查和资源检查 | exit 0；27个 Python 源码语法通过 | `python37-syntax.log` |
| `git diff --check` 和 `archive.py` | exit 0；当前源码/资源/ARM/日志/截图哈希可复核 | `diff-check.log`、`validation.json` |

SDK 宿主 Python 3.7 缺少 sqlite3，导入当前 API 的尝试失败并保存在 `python37-runtime-unavailable.log`；
不把该检查标为通过。目标 ARM 的 SQLite 扩展存在并通过静态依赖检查，但目标 Python/Qt/LinuxFB
尚未实际运行；本轮不修改 SDK 宿主、不用宿主 3.8 成功代替板上 3.7 验收。字体生成未结束时提前
运行校验曾因旧清单报错，保留 `make-test-premature.log`；最终完整测试在生成结束后重新通过。
`font-red.log` 保留修复前的真实 Qt 字形失败。

当前304张Qt截图已重新生成，包含五尺寸14页面及空态/离线/操作位置；查看1024×600显示设置页的
中点、方框和中文实际渲染。未改变布局和交互，完整参考逐图验收仍见第13轮，不冒报本轮重新全图复核。
S10 仅本机软件验收 passed；S01/S11/S12 和所有 H 项尚未完成，GUI 安装仍禁用、S4 保持草稿。
下一原子任务复核 S11 系统输入/存储主机审计；不发送全部软件通过、实板通过、全部完成或全外部阻塞事件。


### 第 16 轮：S11 系统输入、容量和 ext4 持久存储（2026-10-13）

本轮只完成 S11 主机审计，未提交、推送、上传、联网发消息、发布、烧录或格式化；全部历史成果保留。

- 重新读取实际 SDK manifest 的 122 个组件，提交均可读取，7 个组件有本地修改/额外输入；前后清单一致。
  记录脏补丁哈希、修改路径、未跟踪输入以及批准硬件仓库当前状态，批准 S4 板级文件与 dev3 准备快照逐项一致。
  `build/pack`、`mkcmd.sh`、`.buildconfig`、Buildroot 配置及最后 post-build 脚本均绑定哈希。
  SDK 本身仍含其他板配置/修改，批准 S4 硬件在独立仓库，不能沿用当前 SDK SPI NAND 配置打包。
- 清单不是完整可恢复归档：7 个脏组件仍需审查并保存补丁/额外输入，嵌套 squashfs Git 目录需递归清点，
  忽略的下载/工具链/厂商输入仍需精确恢复材料。没有将 SDK 缓存或厂商二进制复制进应用仓库。
- 实际选中 Buildroot 分区 rootfs=1,073,741,824 字节，配置 ext4 镜像=536,870,912 字节，MMC root=p5；
  固定分区=1,130,368,000 字节，32 MiB 开销与至少256 MiB数据的预算下限=1,432,357,888字节。
  实际介质容量未知、完整 rootfs 未提供、vendor `/init` 未安装；只读预检0个配置错误、6个未解决项，
  `publishable=false`。CLI exit 0 只表示配置审计没有错误，不构成打包/发布批准。
- 三个新增红例证明原工具漏检：仅凭 `storage.py --mount` 文字会接受混入格式化的脚本；漏选版本化
  `env-5.4.cfg`；固定 UDISK 很小时只看整盘容量会错误通过。现要求 S60 与 storage.py 完整字节匹配，
  按实际 pack 的版本环境选择及最后 Buildroot env.cfg 覆盖顺序检查 MMC root，固定 UDISK 本身至少256 MiB
  且不重复计入容量预算。报告绑定实际输入/策略哈希及提供的原始镜像字节数/哈希。
- post-build 在写任一策略文件前检查全部目标及父目录符号链接，拒绝向暂存根之外写入。
  实际 shell post-build CLI 在隔离目录覆盖晚到的厂商格式化脚本文本，再审计通过；错误板型拒绝且原文件未变。
  测试只写脚本文本，不执行厂商格式化命令；预存用户数据逐字节保持不变。
- 存储测试覆盖 FAT/未知类型、错设备、只读、叠加挂载、未挂载、挂载失败/超时、挂载后核验失败和恢复。
  失败只允许已有 blkid/mount 路径，没有格式化或自动转换；主机挂载调用为 mock，不实际挂载设备。
  原始 ext4 测试使用明确标记的超级块解析夹具，截断/错误格式拒绝，不把夹具当完整文件系统或可启动镜像。
- 当前磁盘约124 GiB可用，旧的2.3 GiB不足已解除；H02改为pending并登记可继续的本地输入恢复/隔离准备任务。
  dev3仍复用dev2 sysroot，准备目录应用快照仍是历史提交；本轮独立 HMI 不更新完整目标树或历史 provenance。
  当前 finalizer 的符号链接修复尚未进入旧准备快照，后续系统准备必须使用当前源码，不能沿用旧快照冒报通过。

| 命令/验证 | 结果 | 证据（均在 `dist/iteration-s11-system-inputs/`） |
| --- | --- | --- |
| `make test` | exit 0；5+112=117 Python及T153/仓库/资源门禁 | `make-test.log` |
| `.test-venv/bin/python -m unittest discover -s tests -p test_s4_system.py -v` | exit 0；16项系统/存储测试 | `system-tests.log`；修复前3失败见 `audit-red.log` |
| SDK host Python3.7 同一系统测试 | exit 0；16项，非目标ARM运行 | `python37-system-tests.log` |
| `python3 .../cli-audit.py` | exit 0；12次真实 CLI，容量/身份/篡改/原始镜像拒绝及post-build恢复 | `cli-evidence.json`、`cli-*.log/json` |
| `python3 scripts/capture_sdk_inputs.py --sdk ... --output ...` 前后两次；`review-inputs.py` | exit 0；122组件、7脏组件、前后相同；完整恢复包仍false | `sdk-inputs.json`、`sdk-inputs-after.json`、`input-review.json` |
| `python3 scripts/audit_s4_system.py --hardware /home/ubuntu/100ASK_T113s4-SdNand_TinaSDK5 --buildroot /home/ubuntu/T113-tina5v1.2-sdk/out/dshancontrol-s4-dev3/buildroot --output ...` | exit 0；0配置错误/6未解决，不可发布；未猜容量 | `system-preflight.json`、`system-preflight.log` |
| `make -j4 test-hmi` | exit 0；32 passed、0 failed、0 skipped；304张图含五尺寸70标准视图 | `test-hmi.log`、`screenshots/` |
| dev3 qmake + 独立 `arm-build/` 的 `make -j4`；`readelf -h -l -A -d` | exit 0；当前源码，ARMv7 ELF32 EABI5 softfp，实际dev2 sysroot | `s4-build.log`、`arm-elf.log` |
| `git diff --check`、`archive.py` | exit 0；源码/资源/ARM/日志/截图与本地系统输入哈希复核 | `diff-check.log`、`validation.json` |

本轮没有修改 UI，HMI源码/资源与第15轮相同；已查看基准更新页，身份缺失仍如实未就绪、GUI安装禁用。
全页面逐图参考验收仍见第13轮，不声称本轮重新人工审查全部图片。当前Qt截图中的样例值不是实板数据。
S11只标记本机软件审计passed，S01/S12及H项仍未完成；S01与H02可继续的本地工作已登记，
不发送软件全通过、实板通过、全部完成或全部外部阻塞事件。


### 第17轮：S01 产品、应用/系统边界与 T153 一致性（2026-10-13）

本轮只处理 S01。修复 S4 描述引用了不存在的 `t113s4-sdnand-adapter` 组件问题；新增
跨板目录/运行 Manifest 的产品身份、SoC、架构、profile 和未验收能力门禁回归。
组件登记为 board-adapter-example，不增加 S4 supported_profiles、现场总线映射或能力声明。

README 区分历史 T153 深灰实板图与当前蓝白本机界面；架构/移植文档统一 DshanControl
名称和兼容命名空间，明确 API、系统拥有的更新器/信任根/维护钩子、应用与数据目录边界。
S4 独立 SD NAND/ext4 UDISK，不复用 T153 A/B 分区；S3 身份关系继续待确认。
A133/T527/RK3576 仅为未来独立适配目标，EinkOS 仅参考，SmartDesktopMimi 单独开发。
状态文件保留旧提交/63测试的历史身份，引用当前验收表；已解除的空间不足不再写为当前阻塞。

| 验证 | 结果 | 证据（`dist/iteration-s01-boundaries/`） |
| --- | --- | --- |
| 新增跨板目录检查红例 | 修复前实际 KeyError，修复后通过 | `catalog-red.log`、`make-test.log` |
| `make test` | 0；5+113=118 Python及仓库门禁通过，包括真实CLI错产品/系统类型/错板拒绝、签名/包篡改、SIGKILL恢复、健康回退和数据/工厂身份保留 | `make-test.log` |
| T153 历史快照及当前源码接入/安装 | 当前 apply 脚本在临时SDK执行，Web/Core/HMI与字体许可安装回归通过；115个板级/身份输入（含符号链接目标）与HEAD逐字节一致 | `make-test.log`、`boundary-review.json` |
| `make -j4 test-hmi` | 0；32 passed，0 failed，0 skipped；304张Qt图含五尺寸×14标准页 | `test-hmi.log`、`screenshots/` |
| 独立目录使用 dev3 qmake 后 `make -j4` | 0；实际仍使用 dev2 sysroot，ARMv7 ELF32 EABI5 softfp调用约定 | `s4-build.log`、`arm-build/omnigate-hmi`、`arm-elf.log` |
| `git diff --check`及源码/产物哈希复核 | 0 | `diff-check.log`、`validation.json`、`archive.log` |

已查看1024×600实际应用更新页，未知身份如实提示，安装禁用；本轮无UI源码/资源改动，
完整参考视觉复核仍为第13轮，不重新声称实板视觉通过。初次边界审计把symlink当普通文件
与Git链接文本比较失败；已按类型修正审计脚本，保留 `boundary-review-initial.log`，非产品失败。

S01仅本机边界验收通过。未新增T153目标sysroot构建或实板测试；S4构建仅HMI，不是完整镜像。
S12证据汇总已登记在H02本地任务之后；H01/H03/H04/H05缺实板身份、设备、正式信任根、
物理掉电/回退和老化证据；H02仍有本地输入恢复/隔离准备工作。全部既有成果保留，明确禁止
提交优先于通用工作流模板；无提交、推送、上传、发布、联网发消息、烧录或格式化。
不发全软件通过、实板通过、全部完成或全外部阻塞事件。


### 第18轮：H02 可恢复SDK输入与当前源码隔离准备（2026-10-13）

本轮只完成已登记的H02本地输入/准备原子任务，H02整项仍为pending。没有修改应用运行代码，
保留全部既有未提交成果；没有提交、推送、上传、发布、联网发消息、格式化或烧录。

- 实际恢复材料在 `/home/ubuntu/dshancontrol-recovery-20261013-h02/`，厂商源码/二进制/SDK缓存
  均在应用仓库之外。122 SDK组件、嵌套squashfs及批准S4硬件共124个自包含Git bundle，
  保存当前二进制补丁、未跟踪内容和忽略路径清单；哈希全部复核。7个脏SDK组件、嵌套仓库及
  硬件仓库共9个仓库实际克隆、应用补丁并核对恢复文件。记录当前工作文件，不伪造干净应用提交。
- Buildroot下载及应用/内核GCC5.3、Bootloader GCC7.2与RISC-V工具链另行归档、解包、逐文件核对
  内容/链接/权限；工具链来源绑定SDK组件提交、Git blob和原始压缩包SHA256。
  与原压缩包比较：8069+7231+6679=21979个文件/链接相同，内容差异0；GCC7.2仅多`.prepared`
  标记。未独立验证上游下载真实性，不把本地来源哈希当作生产签名或正式信任根。
- 真实CLI拒绝截断bundle，完整bundle随后恢复成功；归档路径穿越/符号链接父目录拒绝且无目录外写入。
  归档中断/失败后按已保存的组件哈希继续，不替换既有成果。全部原SDK组件清单在归档、配置后相同。
- 新准备目录 `/home/ubuntu/dshancontrol-s4-prepared-20261013-h02/` 使用当前应用110文件快照；
  application_dirty=true、board_validation=not-run。独立Buildroot O目录configure成功，未链接dev2/dev3缓存，
  但仍共享原SDK源码和只读工具链，不能称为完整SDK隔离副本。重复输出目录拒绝且原文件不变；
  故意篡改准备快照时build.sh在进入make前拒绝，恢复字节后configure成功。
- 传入其他板型环境进行configure，runner仍固定`t113_s4/100ask`；原SDK `.buildconfig`哈希不变。
  配置选中ARM EABI、Qt LinuxFB/Charts、DshanControl及厂商init包；厂商post-build之后执行本项目覆盖。
  512MiB ext4配置可放入选中1GiB rootfs分区，MMC root仍p5，容量预算下限1432357888字节。
- **新树完整系统预检尚未通过**：真实CLI退出1，尚未安装S60/storage.py产生2个配置错误；
  完整rootfs、厂商/init、真实容量、工厂ext4制备、内核/Bootloader镜像链及实板功能/掉电/回退共6项未解决。
  未造临时target骗过预检，publishable=false；没有执行rootfs all、内核/Bootloader或镜像打包。
  新目录配置后约93.9GiB可用，完整构建和忽略输入依赖闭包仍有本地工作，已登记后续任务。

| 验证 | 结果 | 证据（`dist/iteration-h02-local/`） |
| --- | --- | --- |
| `recover-inputs.py`、`verify-recovery.py` | 124仓库归档、9仓库实际恢复、归档哈希及5项失败/恢复检查通过；原SDK清单不变 | `recovery.json`、`recovery-final.log`、`recovery-verification.json`、`sdk-before.json`、`sdk-after.json` |
| `toolchain-provenance.py` | 三套实际工具链归档/恢复通过，21979文件/链接与原压缩包相同；来源Git blob匹配 | `toolchain-provenance.json`、`toolchain-provenance-stream.log` |
| `prepare-review.py`真实CLI | 新目录prepare/configure退出0；重复目录2、篡改快照1、未完成系统预检1均按预期保留；原SDK未变 | `preparation-review.json/log`、`system-preflight.json/log`、`sdk-after-prepare.json` |
| `make test` | 0；5+113=118 Python及T153/仓库/资源门禁，包括更新错配/篡改/SIGKILL恢复 | `make-test.log` |
| `make -j4 test-hmi` | 0；32 passed、0 failed、0 skipped；304张当轮Qt图含五尺寸×14标准视图 | `test-hmi.log`、`screenshots/` |
| dev3 qmake对新准备快照独立构建、`readelf -h -l -A -d` | 0；ARMv7 ELF32 EABI5 softfp，解释器/lib/ld-linux.so.3；实际sysroot仍dev2，只是HMI目标构建 | `s4-build.log`、`arm-elf.log`、`arm-build/omnigate-hmi` |
| `git diff --check`、源码/产物/外部恢复索引绑定 | 0；最终哈希复核见索引 | `validation.json`、`archive.log`、`diff-check.log` |

过程失败如实保留：首次恢复校验把嵌套仓库缺.git后的分类变化当内容差异，改为文件/链接/权限核对，
嵌套Git另存；Qt大二进制bundle差异计算耗时，终止本轮pack进程后在新105-retry1目录禁用delta搜索重试；
RISC-V首次归档含16硬链接而被严格解包器拒绝，新retry1归档保存独立文件字节后通过。
`recovery.log`、`recovery-resume.log`、`toolchain-provenance.log`为失败/过程日志，不算通过证据。
工具链随机读取xz比较较慢但已成功，随后流式比较复验同样通过；尝试终止时进程已结束，不冒报中断测试。

当前HMI源码/资源与第17轮一致，查看了1024×600应用更新页：缺系统身份/公钥时如实未就绪，安装禁用；
截图为本机样例夹具，底栏与样例值不是实板事实。全页面参考视觉沿用第13轮，不声称本轮重做人工逐图验收。
完整系统/实板/正式更新仍未验收；S12汇总和H02完整构建后续保留，不发全部软件、实板、全部完成或全外部阻塞事件。


### 第19轮：S12 当前工作树软件验收与证据一致性（2026-10-13）

本轮只完成S12，没有修改产品逻辑、UI或板型；全部既有未提交成果保留。S01—S11专项记录继续保留历史日期，
当前汇总为 `dist/iteration-s12-final/validation.json`，逐项需求映射为 `software-matrix.json`。
进入时269个仓库文件与第18轮一致；本轮只更新验收文档，最终索引重新绑定文档、源码、资源及链接身份。
12份历史专项归档共2723产物哈希全部一致，`historical-evidence-audit.json` 明列每轮源码差异，
没有把早期测试计数、字体或ARM产物当作当前结果。第18轮143个恢复元数据与第15轮51个外部目标库哈希一致；
本轮未重读大型恢复归档，不将元数据核验表述成完整系统重建。

| 需求 | 本轮验证 / 结果 | 当前证据（相对于 dist/iteration-s12-final/） |
| --- | --- | --- |
| S01 | T153接入/安装及平台边界回归通过，115历史板级/身份输入与HEAD一致 | `make-test-final.log`、`boundaries/boundary-review.json` |
| S02/S03 | 32 Qt检查通过；五尺寸14视图、300控件44×44、横向溢出/滚动可达及真实取消操作通过 | `test-hmi.log`、`touch-measurements.json`、`visual-review.json`、`index.html` |
| S04 | 缺测/单位/窗口/暂停回归，实际Qt→HTTP六次轮询、告警与SQLite继续采样 | `s04/http-pause-evidence.json`、`s04/live-qt.log` |
| S05 | Qt请求→Flask→CANopen 2.4.1虚拟帧；串口/EtherCAT 18次HTTP，拒绝/超时/恢复 | `can.log`、`serial.log`及对应子目录JSON |
| S06 | 网络选择/能力门禁及19次HTTP，QMI/DHCP失败与恢复 | `network.log`及对应子目录JSON |
| S07 | 18次HTTP含真实Qt确认，SQLite事务回滚/恢复，真实CSV导出/取消/写入失败与回读 | `alarms.log`、`alarms/http-evidence.json`、`alarms/visible.csv`、`alarm-csv.log` |
| S08 | 19次HTTP含Qt设置/背光；服务进程失败/恢复和9条审计；更新身份/进程失败回归 | `settings.log`、`settings/http-evidence.json`、`test-hmi.log` |
| S09 | 身份/渠道/签名/内容/越界/维护/锁/健康回退，10个真实CLI测试含38次调用与两次SIGKILL | `make-test-final.log` |
| S10 | 独立目录S4 qmake/make成功，ARMv7 ELF32 EABI5 softfp，52 ELF加载器/依赖/符号版本通过；当前字体/资源/打包门禁通过 | `s4-qmake.log`、`s4-build.log`、`arm-elf.log`、`runtime-dependencies.json`、`continuity.json` |
| S11 | 当前系统/存储回归与12次真实审计/post-build CLI，错MMC、篡改策略、小容量和符号链接拒绝/恢复 | `system.log`、`system/`、`make-test-final.log` |
| S12 | `make test` 118 Python及仓库门禁、`make -j4 test-hmi` 32 passed/0 failed/0 skipped、目标构建和`git diff --check`均退出0 | `commands.json`、`behavior-commands.json`、`diff-check.log`、`validation.json` |

304张当前Qt截图均由本轮完整测试生成，含五尺寸70标准页、70参考页、70操作位置及基准28空态/离线图。
本轮查看五张原参考图、全部五尺寸参考页与基准空态/离线联系图；未发现新增构图、层级、字体或横向溢出问题。
联系图只是原PNG的本地查看辅助，不是额外运行证据。界面样例数据明确标注；未知身份/未接入明细保持空态，GUI安装继续禁用。
字体独立重建/载荷打包的详细证据仍归第15轮，本轮以未变输入哈希、当前字体/打包测试和新ARM构建衔接，不假称重新生成字体。

归档工具的两次失败均保留：告警HTTP脚本需要本轮CSV而首次Qt命令遗漏输出变量；补跑真实导出后通过。
设置隔离Qt脚本缺截图目录导致保存失败；补建ignored目录后通过。见 `alarms-missing-csv.log`、
`settings-missing-screenshot-dir.log`、`settings/qt-live-http-first.log`；未削弱任何断言或修改产品代码。
`verify.py --self-test` 校验完整归档，并用内存中的错误源码/截图摘要验证拒绝，不篡改已有成果。

本轮只满足本机S项：dev3 qmake仍使用dev2 sysroot，ARM构建不代表独立完整系统或LinuxFB执行。
H02完整构建与ignored输入依赖闭包任务 `spec:h02-system-build` 保持open/pending，空间约94GiB，不能声称全外部阻塞。
H01缺S3/S4关系、实板修订/容量及显示/触摸接线；H03缺冷启动、触摸和外部工业从站；H04缺真实身份、公钥、
维护互锁及物理掉电/故障回退；H05缺稳定性/老化和发布资格。均不以主机结果代替。
本轮不提交、推送、上传、发布、联网发消息、烧录或格式化；仅发软件通过事件，实板/全部完成事件条件仍未满足。


### 第20轮：H02 独立系统构建与依赖输入（2026-10-13）

本轮只完成 `spec:h02-system-build` 的本地构建任务，保留所有进入时未提交成果。
实际输出在 `/home/ubuntu/dshancontrol-s4-prepared-20261013-h02/`，恢复材料在
`/home/ubuntu/dshancontrol-recovery-20261013-h02/`；厂商二进制、镜像及镜像抽取文件均留在应用仓库外。
过程日志、脚本、哈希索引和304张当前Qt截图位于 ignored `dist/iteration-h02-system/`。
未提交、推送、上传、发布、联网发消息、烧录、格式化实际存储或启动其他执行者。

独立Buildroot O目录使用只读SDK、禁网构建和独立下载目录，隐藏原SDK的旧out，仅挂入固定交叉工具链。
恢复内核/Bootloader/固件源码并应用批准硬件125个内核文件和6个无线固件覆盖，编译Linux5.4.61、
板级DTB、8模块、903导出头文件、U-Boot及MMC SPL。U-Boot采用批准BoardConfig明确指定的
`t113s3p`配置名，BSP身份仍是 `t113_s4/100ask`，不据此解决实际S3/S4关系。
SPL预编译库及Xtensa/C906固件是固定来源的厂商输入，不宣称本轮从开放源码编译了它们。

真实构建修复了以下问题，均保留失败日志和旧输出：

- GCC5默认C++98与厂商示例的C++11/GNU扩展不兼容；按具体包阶段设置GNU C++11，
  sdk_common仅预包含cstdlib，真实make测试确认不会污染依赖包或C编译。
- tplayer安装目录与libcedarse工具链选择未覆盖固定的预装softfp GCC5；限定目录/前缀/EABI/glibc，
  错误组合继续拒绝，local-site补丁通过post-rsync应用到构建副本。
- LVGL配方复制SITE外兄弟目录的153个ignored旧对象，造成跳过编译后链接失败；从已验证bundle恢复
  1383源文件并只读挂入，未清理原SDK。41个直接SITE检查不足以单独证明依赖闭包；另核对18处共享头文件引用。
- 普通Buildroot all遗漏厂商packer创建的 `/init → bin/busybox`；按有出处的no-ramdisk规则增加
  S4 target-finalize hook，要求可执行BusyBox及CONFIG_INIT=y，拒绝宿主根、符号链接bin或未知已有init。
  重复执行和失败恢复通过；应用包安装路径未承担系统init修改。
- fakeroot1.20.2在bwrap命名空间尝试真实chown返回EINVAL；启用其内置FAKEROOTDONTTRYCHOWN=1。
  真实chown/lstat探针复现失败后通过，宿主文件属主/内容不变；镜像内root、DBus及PostgreSQL属主均实际核验。

HEAD bundle遗漏版本脚本读取的标签；补存内核/U-Boot/Buildroot已有厂商tags，并在独立恢复树验证。
内核/U-Boot重新编译后最终索引单列；恢复Buildroot标签后setlocalversion由hash后缀恢复为`-dirty`，
与镜像中的2019.02.1-dirty一致。补充材料见 `system-build-tags/`、`system-integration-round20/`
和 `system-integration-round20-final/`。这不等于完整发布复现或逐位可重复构建认证。
170下载输入已记录，缺失DejaVu2.37从Buildroot源镜像获取且匹配厂商配方哈希；构建本身保持禁网。

| 验证 | 结果 | 证据（`dist/iteration-h02-system/`） |
| --- | --- | --- |
| 最终 `make test` | 退出0，5+117=122 Python及T153/仓库/更新器错配、篡改、SIGKILL恢复门禁通过 | `make-test-complete.log`、`make-test-complete-result.json` |
| Qt回归 | 32 passed，0 failed/skipped；304图含五尺寸70标准视图 | `test-hmi.log`、`screenshots/` |
| 独立内核/Bootloader | zImage、板级DTB、8模块；U-Boot及MMC SPL生成 | `kernel-artifacts-final.json`、`bootloader-artifacts-final.json` |
| 第11次完整rootfs构建 | 退出0，生成536870912字节ext4；独立qmake/sysroot的Qt5.11.3/HMI编译安装通过 | `rootfs-attempt11-build.json`及日志、`final-native-review.json` |
| 目标依赖审计 | 1091 ARM ELF的加载器、递归NEEDED和符号版本匹配；2份已固定哈希的非用户空间协处理器ELF单列 | `rootfs-elf-final.json`及日志 |
| `e2fsck -fn` | 退出0，5轮只读检查通过 | `e2fsck-readonly.log` |
| 镜像实际内容 | init相对链接、root/DBus/PostgreSQL属主；抽取25文件逐字节匹配，含安全存储策略、8模块和6无线源固件的安装副本 | `image-content-validation.json`及日志；抽取文件在准备目录validation/image-extracted |
| 最终系统预检 | 带已观测2013265920字节容量复查：退出0、零配置错误；仍有3项未完成，publishable=false | `system-preflight-final.json` |
| 输入连续性 | 原122 SDK组件与.buildconfig未变；110文件应用快照及板级快照不变；新系统配方覆盖单独记录 | `sdk-original-final.json`、`active-build-inputs.json`、`final-native-review.json` |
| 差异与归档 | `git diff --check`及当前源码/产物/外部输入哈希核验；错误摘要拒绝自检 | `diff-check-final.log`、`validation.json`、`verify.log` |

rootfs SHA256：`d55ddb38b66393e09ca17217412673a94b06e2708054c84d416c50b4055b62ed`；
安装后的HMI SHA256：`19ddd16c20dfa56be975d63862388a3b4345d055d0ae3a60a0889eccd8b1e844`。
本轮sysroot为准备目录下新建的 `buildroot/host/arm-buildroot-linux-gnueabi/sysroot`，不再复用dev2/dev3。
原始rootfs为512 MiB，选中分区为1 GiB、MMC p5；交接实测SD容量2013265920字节（3932160×512），超过1432357888字节预算下限，容量预检通过；旧UDISK p9仍为vfat，不符合新ext4数据契约。

HMI运行源码/字体与第19轮一致，当前基准更新页已查看，未知身份与安装禁用如实显示；
完整逐页参考对照沿用第19轮，不把本轮生成截图或组件编译当新增实板证据。
本轮Ralph未执行目标ARM（硬件由主交互代理持有，且无qemu-arm）；旧系统只读观测不代表新镜像启动成功。未新增T153目标构建或实板测试。
`failure-history.json`区分第1—10次失败、预标签产物、曾退出143的测试进程，以及已更正的
镜像固件单路径假设；旧日志不冒充最终通过证据。精确归档计数见 `validation.json`。

归档时发现AGENTS.md/PROMPT.md在长构建期间追加了主交互代理的硬件交接；旧正文前缀哈希一致，
本执行者未修改这两份指引，完整保留新增内容。已读取user-handoff及原始串口只读身份/存储记录，
按实际2013265920字节容量重跑预检；先前无容量预检单列历史，不再沿用“完全无硬件/未知容量”结论。
硬件MCP与试烧授权由主交互代理负责，Ralph仍只做本地构建，不开UART、不操作板端网络/电源/烧录，
不读取私有WiFi凭据。批准DTS的1024×600、51 MHz与交接屏幕方向一致，不构成新镜像显示验收。

本地原子构建任务结束，产物交接索引见 `dist/iteration-h02-system/build-handoff.json`。
H02的Tina厂商完整镜像打包与静态核验仍可本地推进，已登记 `spec:h02-vendor-image-pack` 后续；
不得改走mainline、借用其他板型启动器或串口传文件。H01修订/新身份绑定、H02工厂ext4 UDISK/启动链、
H03新镜像显示触控/工业从站、H04正式身份公钥/维护互锁/物理掉电回退、H05老化仍未通过。
应用快照dirty、正式信任根及打包镜像未完成，保持development-draft。
本轮不发“全部外部阻塞”、实板通过或全部完成事件，只交接本轮完成和本地后续。


### 第21轮：P1硬件新证据与构建输入核验（2026-10-13）

只执行P1交接原子任务，未展开H02完整厂商打包。新增
`boards/t113s4-sdnand/hardware-inputs.md`，修正板级README中“无硬件/容量未知”的过期描述；
接收S4/SD 2013265920字节、root p5、UDISK p9 vfat的旧系统只读证据，
明确Tina厂商包、网络传文件、USB FEL/FES及主交互代理独占硬件的边界。
未读取私有WiFi凭据，未修改主交互代理的user-handoff.json。

本轮没有修改运行源码、BSP、准备目录快照或第20轮镜像。检查已构建DTB而非仅看源文件：
S4与Pro参考均为1024×600/51MHz/rgb18；HBP/VBP分别140/20与160/23，
物理尺寸154×85与280×150mm，后者同样不能作为RGB50实测尺寸。批准S4参数保留，
不复制Pro存储/板型/启动器。Pro参考提交8101582cccd338441a21b9215956be656d47c6d5，
该DTS无本地修改，完整文件哈希见input-review.json。
Goodix触摸仍disabled、残留范围800×480，实际控制器/接线/方向待主交互代理核验。

编译DTB的SDIO1/rfkill/WLAN启用，PG12使能/PG10唤醒；独立内核使用D80固件路径。
从未改动的raw rootfs抽取3个AIC8800 ARM模块（5.4.61 vermagic）及6个D80固件到
应用仓库外的新验证目录，逐字节匹配target与批准S4固件来源。旧系统上电失败、无WLAN接口
仍为未解决硬件事实，不将模块存在当成网络通过。详细58个输入/产物哈希与参数在
`dist/iteration-hardware-inputs/input-review.json`，脚本review.py可只读重验。

| 验证 | 结果 | 证据（dist/iteration-hardware-inputs/） |
| --- | --- | --- |
| make test | 退出0，5+117=122 Python及T153/仓库门禁；含更新器错配、篡改、中断恢复 | make-test.log、make-test-result.json |
| make -j4 test-hmi | 退出0，32 passed，0 failed/skipped，304截图含五尺寸14标准视图 | test-hmi.log、test-hmi-result.json、screenshots/ |
| 新目录S4 qmake/make | 退出0，使用第20轮独立Qt5.11.3 sysroot；ARMv7 ELF32 EABI5 softfp | arm-build-result.json、arm-qmake.log、arm-make.log、arm-elf.log |
| 编译输入与镜像内容 | S4 DTB参数/3模块/6源匹配固件验证通过；第20轮交接组件均重读核验 | input-review.json、module-info.json、input-review.log |
| 容量CLI正反例 | 2013265920字节退出0，零配置错误/3项未完成；1字节退出1、明确拒绝容量不足 | preflight-results.json、observed-capacity.json、undersized-capacity.json |
| 源码保留及证据 | 原运行源码/字体与进入时一致；差异检查及索引哈希验证、错误摘要拒绝自检 | source-before.json、validation.json、verify.log、diff-check.log |

本轮查看基准1024×600操作日志Qt图，完整逐页参考视觉仍沿第19轮记录；没有新视觉改动，
本轮自动量测与截图不充当新实板证据。未执行目标ARM或新增T153实板测试。

交接为 `dist/hardware-mcp/build-input-review.json`，包含组件绝对路径/哈希、外设约束和后续要求；
`vendor_package=null`、`flash_ready=false`，不把raw rootfs当可试烧厂商包。
完整厂商包绝对路径/SHA256/打包输入以及boot/FES/分区/工厂ext4制备核验由已登记的
`spec:h02-vendor-image-pack`完成后写入dist/hardware-mcp。本地仍有可推进工作，不发全部外部阻塞事件。
H01修订/外设接线、H02厂商包/ext4制备/启动链、H03新镜像功能、H04正式身份/公钥/维护/物理回退、
H05老化均未通过；不发实板或全部完成事件。明确禁止提交优先于通用提交模板，未提交、推送、
上传、发布、联网发消息、硬件操作、烧录、格式化或启动其他执行者。

归档首次失败是证据脚本将rootfs.ext4符号链接文本与镜像字节哈希混比；已确认厂商链接目标为
rootfs.ext2，按外部输入语义校验实际文件字节后通过，原镜像未改。失败日志
verify-first-failure.log保留；Python3.8的Path.readlink兼容与一次Qt测试文件路径误读均已记录fix memory。
最终索引覆盖271源码、342本地产物、58输入/组件文件，含304张Qt图；错误源码/截图摘要拒绝自检通过。

### 第22轮：H02 Tina厂商完整镜像打包与静态核验（2026-10-13）

只完成 `spec:h02-vendor-image-pack`，本轮没有应用运行源码/UI或原SDK修改。独立打包目录
`/home/ubuntu/dshancontrol-s4-vendor-pack-20261013` 从第18轮bundle/补丁/未跟踪归档恢复
build、common配置、S4配置、tools四组件，叠加批准S4硬件输入。核验第20轮组件哈希，
使用其内核/zImage/DTB、U-Boot、MMC SPL和512MiB raw rootfs；同一S4源新增FES与通用
厂商容器要求的NAND boot0构建，真实启动仍是MMC p5，不借其他板型loader。
厂商mkkernel.sh原函数计算zImage解压避让，生成无ramdisk的boot.img v2，DTB地址0x4113f100。
打包在原SDK只读、禁网、无物理设备的bwrap环境执行；原SDK其他板型.buildconfig未采用。

最终厂商镜像：
`/home/ubuntu/dshancontrol-s4-vendor-pack-20261013/out/t113_s4/100ask/buildroot/t113_s4_linux_100ask_uart0.img`，
1426076672字节，SHA256 `2da05c73f0a4f6e98bd0b6033871825f3a95d6d9aa35c3ba98401f18c783570e`。
这是完整Tina IMAGEWTY容器，不是重命名的rootfs。原厂脚本实际暴露三处系统输入问题：

- GPT生成写死61079552扇区；仅隔离脚本改为实测3932160扇区，保留40960逻辑偏移，
  最终真实GPT的分区起点为73728扇区，不能仅沿用预检的32MiB估算预算。
- 原UDISK无下载载荷；仅给最终UDISK增加udisk.fex，固定分区大小/顺序不变。
  使用本轮独立Buildroot的mke2fs创建**普通主机文件**，768MiB ext4/root属主，
  除lost+found为空；最终p9分区845132288字节。没有格式化任何板端或宿主块设备。
  本包是工厂系统制备输入，写入空UDISK会替换卷数据，不能当作保留数据的应用更新。
- 默认image.cfg要求arisc.fex，但厂商build_arisc在S4无arisc.config时直接返回；
  删除该无来源条目，保留真正DSP/amp_rv0分区；实际使用Buildroot的default/image.cfg，
  单改image_linux.cfg无效。完整patch/来源/有效配置均保留。

| 验证 | 结果 | 证据（dist/iteration-h02-pack/） |
| --- | --- | --- |
| 厂商pack第5次 | 退出0，Dragon SUCCESS，完整IMAGEWTY镜像生成 | pack-attempt5.log/json |
| 最终容器检查 | 41条目逐项匹配暂存载荷SHA256、无越界；GPT/4份MBR/下载表CRC、9分区/8下载项、root p5/UDISK p9通过 | package-inspection.json、inspect-package.py/log |
| boot链静态检查 | boot.img中zImage/DTB与独立构建精确匹配；MMC env CRC/参数、FES/MMC及NAND boot0校验和、TOC中U-Boot/OP-TEE/DTB完整匹配；打包DTB与原DTB解码一致 | package-inspection.json |
| 文件系统 | 512MiB rootfs与第20轮哈希相同；768MiB ext4数据载荷在下载表中；两个实际打包载荷只读fsck通过、空数据卷属主及特性检查 | packed-rootfs-fsck.log、packed-udisk-fsck.log、udisk-directory.log、udisk-superblock.log |
| 真实拒绝检查 | 31GB旧容量、过小容量、GPT CRC损坏、有效CRC但p5/p9错配、截断容器、真实rootfs载荷单字节篡改均拒绝；原包重新验证成功 | adversarial-results.json/log；损坏副本仅在仓库外 |
| 恢复后重打包 | 1553个输入条目含权限/链接实际归档恢复；在恢复树重新pack退出0，最终镜像逐字节一致 | recovered-pack.json/log、recover-repack.log |
| make test | 退出0，5+117=122 Python及T153/仓库门禁，含错配/篡改/SIGKILL事务恢复 | make-test.log、checks.json |
| make -j4 test-hmi | 32 passed、0 failed/skipped，304张Qt图含五尺寸14标准视图 | test-hmi.log、screenshots/ |
| 独立S4 HMI交叉构建 | 新arm-build目录，使用第20轮独立Qt5.11.3 sysroot；ARMv7 ELF32 EABI5 softfp | arm-qmake.log、arm-make.log、arm-elf.log、checks.json |
| 原输入/证据 | 原122 SDK组件捕获与第20轮完全一致；原系统交接组件哈希未变，22个HMI文件与第19轮视觉验收相同 | sdk-original-final.json、preparation.json、source-before.json、visual-continuity.json |
| 归档与差异 | 当前源码、日志、截图、镜像/输入摘要绑定；拒绝错误归档摘要；git diff --check通过 | validation.json、verify.log、diff-check.log |

恢复材料保存在 `/home/ubuntu/dshancontrol-recovery-20261013-h02/vendor-pack-round22/`。
归档的是可实际重打包的完整物化输入，补充第18/20轮源/工具链恢复材料；不声称从全空环境
重建完整系统、跨主机逐位复现或实板通过。厂商工具/镜像/损坏副本均在应用仓库外。

失败全部保留：首轮因恢复树无声明标记而EOF无限循环，主动SIGTERM停止；已证明原SDK
有相同三份条款的确认记录，仅复用空标记，不读/复制原确认文件可能含秘密的环境内容。
第2轮缺FES/NAND产物、第3/4轮ARISC条目失败；首次同时make fes nand导致共享config竞争，
保留失败树后逐目标串行交叉构建成功。每项实际失败已记fix memory，没有用假载荷补缺。
打包的“请config”警告来自未启动厂商buildserver进程；本轮明确绑定独立配置和所有输入，
未为消除提示启动背景服务。未把这些历史失败改为通过。

已查看当前基准“系统身份缺失”Qt图；应用安装仍禁用、身份未就绪如实显示。
本轮无视觉改动，完整逐页参考视觉沿第19轮；当前304图与自动量测仅为主机回归，非实板证据。
未执行目标ARM程序或新增T153实板验收。交接见 `dist/hardware-mcp/vendor-package-handoff.json`，
包含外部镜像绝对路径/SHA256、完整分区/启动/制备输入、当前静态证据及未完成事项。
原user-handoff.json与第21轮历史交接未覆盖。

本地打包原子任务完成，H02整项仍blocked_external：必须由主交互代理重新识别绑定板，
按已有用户试烧授权完成实际USB厂商写入、冷启动及ext4 UDISK挂载验证。
H01板号/修订和面板/触摸接线；H03新镜像RGB50/触摸/AIC8800D80及工业从站，旧WiFi上电失败；
H04正式身份/信任根/维护互锁及物理掉电回退/数据；H05老化仍未验收。
后续仅外部/实板证据工作，不能发实板通过或全部完成。明确禁止提交优先于通用模板；
未提交、推送、上传、发布、联网发消息、硬件操作、读取WiFi凭据、烧录或格式化设备。

## Hardware retry after USB port change

- User changed the USB port and physically entered FEL. Project/device 7 binding:
  `pciroot(0)#pci(1400)#usbroot(0)#usb(10)#usb(2)`.
- Vendor package SHA256 remains
  `2da05c73f0a4f6e98bd0b6033871825f3a95d6d9aa35c3ba98401f18c783570e`.
- Task `flash-1791948657068341400` passed FES reconnect and performed real erase/write.
  Final LYNX status: success, 100%, 1362353584 committed bytes, verification success.
  Boot0/Boot1 verification is reported in the task log.
- Raw CLI tail reports device-not-found on a post-action retry, while LYNX reports
  post-action success. Serial independently confirms a reboot, successful boot0
  loading of the boot package, then OP-TEE hardware-check panic before U-Boot/Linux.
- Blocking error: `check_hardware_info:114 hardware check error1`, followed by
  `plat_init` panic at `core/arch/arm/plat-sun8iw20p1/main.c:332`.
  `huk not found` also occurs in the old successful boot, so that warning alone
  does not establish the cause. Packed OP-TEE matches the specified S4 vendor
  repository binary (SHA256 `5fd64b53603f3077b73452f3f509f47dc89283e883d4803cc169c54754fc6d61`).
- H02 remains blocked on startup; ext4 mount, desktop, display/touch/WiFi and
  physical rollback acceptance remain unverified. Do not publish this image.
- Evidence: `dist/hardware-mcp/port-retry-flash-live.json`,
  `port-retry-progress.jsonl`, `port-retry-uart.log`, `trial-flash-status.json`.
  Primary agent closed its serial monitor after capturing the failure.

## 2026-10-14 — corrected Boot0 and actual application startup

Root cause confirmed by correction and hardware boot: the isolated pack omitted
vendor buildserver; update_chip could not obtain its parameters, and the pack
ignored its failure. Boot0 hardware information stayed zero. Keep the original
S4 OP-TEE; no OP-TEE bypass or other-board loader was used. See the reproducible
correction and final-container gate in `boards/t113s4-sdnand/hardware-inputs.md`.

- Corrected image SHA256: `535918fe291d8fe9c707f9afc75bdb1dfd0c3e48b77376048665c35e569bd205`.
- Network transfer `transfer-1791950592209453800` succeeded with matching SHA256.
- Flash `flash-1791950677798869600`: success, 100%, 1362353584 committed bytes,
  verification success. UART independently confirms reboot into Linux 5.4.61,
  root ext4 on p5, no OP-TEE panic. A raw CLI post-action retry still reports
  device-not-found after the first reboot; do not mistake that for boot failure.
- RGB framebuffer reports `U:1024x600p-59`. Visual/touch acceptance remains pending.
- Auto-mount initially refused UDISK: BusyBox `CONFIG_FEATURE_BLKID_TYPE` is unset,
  so blkid reports label/UUID but omits TYPE. The init gate failed closed as designed.
  This is a build configuration defect, not missing factory ext4 provisioning.
- An explicit `mount -t ext4 -o rw,nosuid,nodev /dev/mmcblk0p9 /mnt/UDISK`
  succeeded without formatting. Then S70dshancontrol initialized the factory app.
  Web, native HMI and supervisor processes run; the system health hook returns 0,
  including application identity and database checks. This was manual startup;
  automatic cold-boot mounting/startup is NOT yet accepted.
- The vendor /dev/null node is 0660 and causes postgres initialization errors;
  factory service cleanup/device permissions remain system integration work.
- New host validation passes: 5 core + 121 tests; old failed final image is rejected
  by the Boot0 gate, corrected image passes; pack failure patch applies cleanly to
  the original build component and patched shell syntax is valid.
- Evidence: `dist/hardware-mcp/bootfix-*`, `trial-flash-status.json`,
  `dist/iteration-h02-bootfix/`. No commit/push or publication. H02/full-board
  qualification remains incomplete due to automatic startup and other device tests.


## 2026-10-14 — RGB50 touch diagnosis and durable correction

- BusyBox TYPE support now permits automatic UDISK ext4 mounting. On pin-fix
  image `f6ed9c2c...`, the application health check passed after automatic boot.
- Enabled I2C2, relocated the Goodix child, resolved PE13/PHY-reset and PE12/OTG
  conflicts. Flash `flash-1791956568699193300` passed verification and Linux boot.
- GT911 responds at 0x5d with product bytes `39 31 31 00`; 0x14 NACKs.
- Vendor I2C drv/DMA mode fails the long configuration read. Runtime engine-mode
  test reads all 240 bytes and registers Goodix with ABS_MT X=0..1024, Y=0..600.
- After a controlled service restart, Qt uses evdev and opens the named Goodix
  `/dev/input/event5`; the health hook returns 0. This alone is NOT proof of
  touch interaction. The first 45-second capture saw zero events, and the user
  has not yet confirmed a physical tap during capture. IRQ/UI testing remains open.
- The patch now fixes 0x5d and engine mode in DTS for reboot persistence. Combined
  image `ac593abd7666393c2b7e554ea8d775b7255fb7c75e8868b8f6c505159ae233bc`
  passes final-container inspection and Boot0 gate; durable boot test pending.
- Tests: `make test` passed after runtime/post-build/tool changes (5 core + 121);
  subsequent DTS edits compile and apply without fuzz; `git diff --check` passes.
- Evidence: `dist/iteration-h03-touch/`, `dist/hardware-mcp/touchpins-*`,
  `touchengine-*`. No Git commit, push or public release. Full-board acceptance
  remains open (touch events, WiFi/network, industrial I/O, rollback, visual checks).


### V1.3 carrier jumper gate — latest physical blocker

The complete local V1.3 base schematic, page 7, confirms PB2/PB3, not PE0/PE1.
J14 routes PB2 between CTP_RST and CAN0_TX; PB3 between CTP_INT and CAN0_RX.
Touch requires shorts 1–2 and 4–5. CAN0 selects 2–3 and 5–6. Inspect the board's
pin-1 mark and revision before applying this mapping; power off before moving
jumpers. These shared routes cannot claim simultaneous native CAN0 and touch.

The engine-mode image flashed/verified successfully and booted with GT911 at
0x5d, correct 1024x600 ranges, automatically mounted UDISK, and health=0. However,
the user explicitly reports no response after tapping. The configured PB3 IRQ
count remains zero, so touch is NOT accepted. The schematic makes J14 routing a
specific item to check; its actual physical position has not yet been confirmed.
PE1's single edge on requesting observation is not evidence of a touch IRQ. PE0
held low does not suppress GT911 I2C reads, further rejecting the old tutorial's
PE0 reset mapping on the currently connected hardware. Diagnostic GPIO handles
and raw I2C capture were released; PE0 restored to input; desktop left running.
The user is powering off to reconnect jumpers; await power-on before more access.

Schematic source: `/home/ubuntu/T113-tina5v1.2-sdk/eink-sch/100ASK_T113-Pro_Base-SCH_V1.3.pdf`,
SHA256 `2100fa2decf074978107358d01c875081f9acd7233bcbf670380c54833234a4e`.
Reference URL: https://dl.100ask.net/Hardware/MPU/100ask_t113-pro/100ASK_T113-Pro_Base-SCH_V1.3.pdf
Evidence: `dist/iteration-h03-touch/j14-schematic.png`, `base-complete.txt`,
`dist/hardware-mcp/touchengine-probe.json`, `touchengine-diag-cleanup.json`.


### 2026-10-14 — touch recovered; brightness interaction follow-up

After power-off/reconnection the user confirmed the desktop is operable and
clarified that only the brightness slider is awkward. PB3 interrupts increased;
Qt opened Goodix event4, health=0, and capture recorded 5103 real press, motion and
release events. This supersedes the earlier zero-IRQ/J14-wait blocker; exact
physical jumper positions were not independently photographed or inspected.
Evidence: `dist/hardware-mcp/j14-recheck-probe.json`, `j14-capture-read.json`,
`j14-events-network.json`. Global touch calibration remains unchanged.

The brightness control now accepts absolute taps across a 48-pixel-high target,
retains the finger offset when grabbing its enlarged handle, clamps drags at the
ends, and offers 44x48 minimum-size +/- controls with a 3/255 adjustment step.
The existing save/confirmation flow remains in use. `make test-hmi`: 33 passed,
including touch, keyboard, dirty-state preservation and responsive layout tests.
`make test`: 5 core + 121 Python tests/gates passed. Cross-compilation with the
T113 Buildroot Qt/sysroot passed. Actual 1024x600 Qt screenshot reviewed locally;
artifacts under ignored `dist/iteration-h04-brightness/` and `dist/ui-test/`.
Physical comfort of the revised slider still needs user confirmation.

AIC8800D80 modules already present in the vendor image were loaded for this boot:
`aic8800_bsp`, then `aic8800_fdrv`. SDIO detection and firmware loading succeeded;
wlan0 associated with the authorized lab WiFi and DHCP assigned 192.168.1.64.
Credentials were configured with console echo disabled and are not in evidence.
Board-initiated HTTP downloads reset; a one-shot TCP push over WiFi succeeded
(observed peer 192.168.1.39, rather than SDK ens33 address 192.168.1.199).
The entire 7761920-byte trial archive was verified against a separately pinned
SHA256 before extraction: b17d12569d498131163da4ec04ad9bdead7121ffadeebf6980d5250c08f5e229.
No file payload was transferred over UART. Network startup/persistence is not
accepted by this one-boot connection test.


Brightness trial deployment completed: PID 2942 runs
`/tmp/dshancontrol-brightness-trial/omnigate-hmi`, SHA256
`f6653fe3bc1766b1a5b1684839c223d2a5455d3d4e1d6bb61609a14a5ef10298`.
The trial retained the old HMI environment, opened framebuffer and input, remained
running through the startup observation, and system health returned 0. Only the
HMI process was replaced; Web/supervisor were not stopped. Installed release
metadata and version-tree contents remain untouched. This is a temporary lab
trial, not a signed application update. Reboot restores the preinstalled HMI;
immediate restore command: `python3 /tmp/dshancontrol-brightness-trial/try-hmi.py restore`.
Physical slider comfort still requires user feedback. Evidence:
`dist/hardware-mcp/brightness-trial-health.json`. Nothing committed or pushed.


## 2026-10-14 — C++ 更新运行端与主板队列

用户新增后续适配 IMX6ULL-PRO、STM32MP157、RK3506，已记录在 BOARD-PORTING-QUEUE；
均待适配，不声明支持。用户要求量产框架使用原生语言，本轮选择 C++11/Qt，撤回
本轮新增的 Python 平台服务/任务原型，仅在 ignored dist 保留工作参考；此前代码恢复。

新增 components/runtime 原生更新核心、Unix RPC 独立任务服务及 CLI；通用应用载荷
组装抽为 scripts/application_payload.py。未切换生产系统、上传/发布或改写旧版本记录。
硬件操作完整迁移、原生策略/UI、正式信任根/维护互锁、发布矩阵及实板完整 OTA 尚未完成。

验证：18 原生测试通过（复用 CLI 故障/恢复矩阵并新增真实服务/断连/身份/阶段检查）；
make test 退出0，5+139 测试及仓库门禁通过；S4 ARM 交叉构建退出0；git diff --check通过。
板端身份重新确认为 allwinner,t113_s4/armv7l，WiFi 192.168.1.64，实际应用 health=0。
317440 字节合成签名试验包通过 WiFi 单次推送，完整 SHA256 校验：
f42299b54b58559a60feb0437081c6ce1bf2ebc4a1078b304ee5cc135518909d。
隔离 native-runtime-lab ext4 目录完成原生安装、重复包拒绝、数据及实际系统身份保留，
退出0；这是 core/ABI 试验，不是当前真实桌面升级或掉电验收。

证据：dist/iteration-h05-platform-update/{all-tests.log,native-tests.log,native-arm-build.log}，
dist/hardware-mcp/{native-board-probe.json,native-network-verified.json,native-board-smoke.json}。

### H05 后续验证与亮度平台迁移（2026-10-14）

本节更新上节的阶段性数字与范围，原历史记录保留。原生更新核心补充生产挂载契约
检查与有界子进程退出约束；独立服务增加真实 SIGKILL 后事务恢复/残留 socket 重连
验证。HMI 接通检查、下载、安装确认和任务阶段/原始错误码，34 项 UI 测试通过。

新增 C++ display.brightness.get/set，系统固定显示通道和背光驱动，匹配板型/架构，
按 max_brightness 换算并读回配置值。客户端路径覆盖、错通道、错板、未确认、越界
和非整数请求拒绝；写入与更新器共用 control_lock，维护期间拒绝并且不自动重试。
Web 在显式配置原生 Unix 通道后转发亮度，失败不回落直接写 sysfs。生产默认启动、
硬件完整审计及非特权应用隔离尚未接入；其他硬件操作仍待迁移。

最新验证：23 项原生测试、33 项 Web 回归通过；make test 退出0，5 核心 + 145
其余测试及全部仓库门禁通过。S4 原生 ARM 交叉构建退出0，最新二进制 SHA256：
1d7be8d056ef2241bfef83a56386fb3777451ccc21e753202b76780612293717。
最新亮度接口验证为主机 fixture 和交叉构建，尚未进行该接口的实板操作验收。
日志：dist/iteration-h05-platform-update/{native-display-tests.log,
native-display-web-tests.log,all-tests-native-display.log,native-display-arm-build.log}。

此前 S4 独立服务试验通过本地 RPC、身份/状态、重复实例拒绝、SIGKILL 后残留 socket
恢复，实际产品 health=0，系统及出厂应用记录哈希保持一致。其网络试验归档 SHA256：
83845e07b0419d553c35ad0104116b4b69ba288e35e89bfa7b437d0803d6899a。
证据：dist/hardware-mcp/{native-service-network.json,native-service-board-probe.json}。
该归档早于最新存储/子进程约束及亮度改动，使用 fixture-only 契约和合成包，不能
作为最新程序、真实桌面升级、正式系统基线、物理掉电或整体量产 OTA 验收。

当前代码位于本地工作树，未新增提交/推送/发布；完整应用升级闭环仍未验收完成。

### 整理上传与提交后本地验证（2026-10-14）

用户授权“记录上传，然后本地测试”。本轮将此前待提交的蓝白 UI、S4 厂商集成
修正、C++ 更新/任务/亮度框架、测试及发布门禁一并整理到现有开发分支
feat/t113s4-sdnand-application-updates；新预览、固件、原始本地日志、签名私钥及
设备凭据不进入提交。同步 README、当前 H01—H05 状态和 release-status，旧记录
保留历史含义。GitHub 主机门禁同时加入 Qt UI 测试及其构建依赖。
该 CI 改动已准备但仅保留本地：GitHub 当前凭据缺少 workflow 权限，服务器拒绝
包含工作流修改的首次推送。先上传代码、测试及记录，远端 CI 的新增依赖/Qt 步骤
待工作流权限补齐后上传；不能把这次本地测试描述为远端 CI 通过。

上传后复跑 make test、make test-hmi，以及当前 T113 sysroot 下的原生运行端/HMI
交叉构建；实际命令、退出码、被测提交及结果随后追加。本次代码推送不创建正式
Release，不将实板完整 OTA、物理掉电回退或未来主板适配标成通过。

上传后验证完成：代码提交 6dcd4b3d8e86e838de7fad866503b2a51fee28c9 已推送，
git ls-remote 核对同一分支/提交。make test 退出0，5 核心 + 145 其余测试（含
23 原生案例）及仓库门禁通过；make test-hmi 退出0，34 passed/0 failed/0 skipped。
在新的 dist/upload-20261014/native-arm 与 hmi-arm 目录使用 dev3 qmake（dev2
既有 sysroot）分别重新构建运行端和 HMI，均退出0，产物为 ARM ELF32/EABI5。
运行端 SHA256：1d7be8d056ef2241bfef83a56386fb3777451ccc21e753202b76780612293717；
HMI SHA256：056beb525054e863bfd46945f5ae8599f9c3d6b02600ddea51930d77afc73d88。

被测提交、日志哈希、实际产物及剩余门禁见
[本轮结构化验证记录](validation/2026-10-14-native-updates.json)。测试源代码匹配该提交，
工作树仅 CI 工作流补丁另有本地改动；本次未新增实板烧录/应用升级/掉电操作。

远端 [Actions 38545421668](https://github.com/dshanpi/DshanControl/actions/runs/38545421668)
实际失败：make test 的原生构建报 qmake: not found。所需 Qt/OpenSSL/zlib 依赖和
Qt UI 步骤已经在本地工作流中准备，但现有 OAuth 凭据缺少 workflow scope，GitHub
拒绝上传工作流。该待办须补齐凭据权限后上传并复验，不能记成 CI 通过。

### CI 工作流上传恢复（2026-10-14）

用户要求继续处理 CI。进一步核对发现本机已有 GitHub SSH 登录；标准 22 端口
连接被关闭，GitHub 官方 ssh.github.com:443 路线认证成功。使用原已保存的
github.com 主机密钥别名并保持 StrictHostKeyChecking=yes，通过 SSH 推送同一
开发分支即可提交工作流，无需扩大现有 OAuth token 权限。此前只核对 HTTPS
凭据的结论不完整；其权限错误记录仍保留当时含义。

工作流新增 qtbase5-dev、libqt5charts5-dev、libssl-dev、zlib1g-dev，再依次执行
make test-bootstrap、make test、make test-hmi 和 git diff --check。此次修改仅补
构建依赖和验证步骤，不跳过原生/UI 测试，也不将实板 OTA 未验收改成通过。
推送后等待远端真实执行并记录结果。

首次 SSH 上传后的远端运行 38546563546 通过依赖安装及 make test，Qt 汇总为
33 passed/1 failed：brightnessTouchInteraction 在新 runner 中保存截图失败。
原因是截图目录由后续布局测试创建，亮度测试隐含依赖本地旧目录。修正 Qt 的
initTestCase 显式创建输出目录并检查成功，所有截图/触摸断言保持；在新的空工作
目录运行完整 Qt 测试，再推送复验远端。此修改仅测试准备，不改变应用代码或
之前 ARM 构建的适用范围。

修正后本机在全新空目录执行完整 Qt 测试，34 passed/0 failed/0 skipped，退出0。
证据 dist/ci-fix-20261014/clean-ui-test.log；未复用原 dist/ui-test/screenshots。

后续远端运行 38547340340 的亮度案例已通过，make test 仍通过；Qt 的 CSV 导出
案例失败（提示完成但指定文件未出现）。固定测试为 Qt 文件对话框，分开设置输出
目录/文件名，等待一个事件轮次并确认 selectedFiles 精确匹配后接受；增加有界等待，
CSV 内容、导出快照、取消保留旧文件及写入失败断言均保持。全新工作目录及独立
XDG_CONFIG_HOME 的 CSV 专项验证退出0，未修改产品导出功能。
同样独立的完整 Qt 验证退出0：34 passed/0 failed/0 skipped，日志为
dist/ci-fix-20261014/all-clean-test.log，随后上传继续验证远端。

CI 待办已解决。提交 efe094a37d33b14491d0f08d2d562ef9b7c1f3a7 的
[远端运行 38548208873](https://github.com/dshanpi/DshanControl/actions/runs/38548208873)
已完成且 conclusion=success。依赖安装、make test-bootstrap、make test、
make test-hmi、git diff --check 均通过；实际日志为 5 + 145 项主机测试及门禁、
34 passed/0 failed/0 skipped Qt 测试。此次修正没有改变此前本机及 ARM 验证的
应用运行代码，Git diff 已核对 components/scripts/boards/platform/Makefile 无差异。
结构化记录见 [CI 验证记录](validation/2026-10-14-ci.json)；原失败记录保留历史。

现有 GitHub SSH 授权已用于该仓库的 push URL，主机密钥严格校验保持开启。
路线参考 [GitHub 官方 SSH 443 说明](https://docs.github.com/zh/authentication/troubleshooting-ssh/using-ssh-over-the-https-port)。
CI 配置创建、上传和远端软件验证均完成；真实桌面实板升级、重启持久性、物理掉电/
失败回退、数据保护以及正式系统基线仍须按 H04/H05 验收，不由 CI 成功代替。

### Actual-board update trial started（2026-10-14）

用户要求继续实板操作。重新确认固定 MCP project/device 7、COM44 与 USB 位置，
无并发硬件任务；实板仍为 allwinner,t113_s4/armv7l，WiFi 192.168.1.64，UDISK
为 mmcblk0p9 可写 ext4，当前出厂应用提交 52d05e9，产品 health=0。真实硬件修订、
正式公钥/维护接口仍未配置，不绕过生产匹配门禁。实机后续试验和生产验收分别记录。

修复原生亮度对厂商系统的适配：S4 没有标准 backlight0，使用已核对 /dev/disp
LCD0 UAPI。C++ RPC 实测 100→160→100，锁占用拒绝第二次控制且不重试；出厂应用
与系统身份哈希保持一致，health=0，退出0。24 项原生测试、151 项主机测试及门禁、
S4 交叉构建通过；证据 dist/board-ota-20261014/ 与 ota-native-display-physical-result.json。
真实应用包安装/回退/重启试验继续进行，不能标成完整生产 OTA 通过。

为实机试验的系统启动器补充 HMI 配置/客户端环境入口，保持旧基线缺省路径。
既有原生更新任务测试改为通过这两个启动参数接入真实客户端进程，下载来源、
安装确认、错误/失败不重试断言保持；34 项 Qt 测试及 S4 HMI 交叉构建通过。
系统配置和可信更新服务仍决定目标身份与权限，界面路径不构成安装授权。

### Full-application physical native update pilot（2026-10-14）

源码 `4f2a301fd9aeebb7337bafa99271a14b46d11346` 的完整 Qt HMI、Web、监督服务、资源与许可
构建成三份序号 1/2/3 的签名开发包。私钥在主机临时目录签名后删除，仅公钥送到设备。
原生二进制 SHA256 `2ce162040811c28e8c18237cc8e2ea90d0b5a97759cd27fac9ba654c94e87cfd`，
HMI SHA256 `9feb11ee9807cb439c32569ab3e2a29c729713e1df43933eda917f56b2c87ade`；
从已提交源码强制重建 HMI 得到相同字节。全部文件载荷经 WiFi 传输并校验，UART 仅命令/日志。

设备试验根目录 `/mnt/UDISK/dshancontrol/native-ota-pilot-20261014`，更新契约明确为
`fixture-only-native-ota / fixture-only-unverified`，没有把待确认 PCB 修订写成生产身份。
原始系统契约、公钥位置及出厂版本指针未修改；当前运行的是试验版本和原用户数据的本地副本。
系统侧独立原生程序负责安装/恢复；临时生命周期、存储和健康工具仍复用旧 Shell/Python。
不能据此声称全部生产运行栈已经迁移到 C++。

| 实机检查 | 结果与范围 |
| --- | --- |
| 序号 1 健康失败回退 | 实际健康检查发现临时系统启动脚本导出的应用提交仍是原出厂提交，拒绝新版本；退出 1、health/health-failed，自动回退到 factory，未重新安装失败版本 |
| 序号 2 完整应用安装 | 修正系统脚本读取试验 current 的提交并去掉未使用的序号 2 健康注入；包未改动。安装退出 0，运行 `2-7a6fb9f86bdb662c`，HTTP、数据库、HMI framebuffer/input 描述符检查通过 |
| 序号 3 物理断电恢复 | 确认持久事务为 switched，在受限 45 秒 start 窗口调用已保存单继电器 GPIO6 的一次 cycle/off1000ms；冷启动日志、boot_id 变化和 uptime 重置证明实际重启。原生服务开机回退到序号 2，health=0、事务清理、高水位保持 3 |
| 防重放 | 显式提交序号 3 作拒绝测试，verification/rejected；当前版本仍是 2。不是自动重试失败硬件任务 |
| 正常冷启动 | 修正 WiFi 关联/DHCP 顺序后再次物理 cycle，应用自动运行、health=0，WiFi 自动取得 192.168.1.64；仅临时系统启动层通过 |
| 数据保护 | 原系统/出厂身份及原用户数据哈希复核不变，试验数据目录中的哨兵跨安装/回退/断电保留；没有验收生产数据迁移 |
| 原生亮度 | 实际 RPC 100→160→100、锁冲突拒绝不写；新版本 Web→原生服务 140→100 通过。读回为驱动配置值，不冒充物理亮度测量 |
| 原生 GitHub 检查任务 | 独立服务任务 ec115b039ffa44fcacf1696f97ecdd03 success，elapsed_ms=1558，候选为空；没有验收真实 GitHub 下载安装 |
| 软件验证 | 151 主机（5+146，含 24 原生）、34 Qt、S4 原生/HMI 交叉构建通过；源码 4f2a301 的远端 CI completed/success：[运行记录](https://github.com/dshanpi/DshanControl/actions/runs/38556315919) |

物理断电试验只覆盖一个切换阶段，未覆盖全部写入阶段/老化；应用安装由桌面外独立 CLI 执行，
GUI 下载/安装与来源绑定仍只有主机测试。健康失败返回通用 health-failed，生产回调原始退出码
及功能级 HMI/工业健康检查还需要完善。原生服务权限目前为 root 试验配置，正式最小权限仍待落实。

当前启动层 `/etc/init.d/S70native-pilot` 独立于应用包；原 S70/S71 已备份并暂时隐藏。
WiFi 私密配置位于试验系统目录、权限 0600，通过固定证书校验的双向 TLS 传输，不进入 Git/应用包。
临时 UTC 种子仅用于本轮 TLS 试验，不能当作 RTC/NTP 或时间有效性验收。

恢复原系统启动入口的工具已通过网络送到设备，`sh -n` 和实际 `--check` 均退出 0：

```sh
/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/restore-original-system.sh --check
/mnt/UDISK/dshancontrol/native-ota-pilot-20261014/restore-original-system.sh --restore
```

工具先核对固定 S4、存储、备份、任务、进程所有权和原始身份/数据，之后停止试验服务，
恢复原 S70/S71 启动入口及 factory 应用。未执行 `--restore`，设备继续运行已验证的序号 2。
试验数据独立保留；恢复后下次开机不再使用试验 WiFi 自动连接层。工具仅为本次固定设备的恢复入口，
预检查成功不等于已实际执行/验收恢复流程。

结构化源版本、包/二进制哈希、当前状态和本地证据索引：
[2026-10-14 实板原生更新记录](validation/2026-10-14-board-native-ota.json)。
原始日志/包/私密配置保持本地，未创建公开 Release。H04 继续 in-progress，
`automatic_updates_qualified=false`；正式硬件身份、信任根/维护互锁、完整 GitHub 闭环、
数据迁移、掉电矩阵与老化仍是发布门禁。

### Physical pilot continuation: health codes, Qt task and prepared recovery（2026-10-14）

继续固定 project/device 7、COM44、USB(10)/USB(2) 的 S4 实板操作，厂商镜像没有重新烧录。
上一轮序号 2 起步；新增真实签名应用包 4/5/6，当前运行序号 6、应用提交
`26088ef1acbc1bf8bf19e6c586d58e38c408a93e`，高水位 6，失败列表为 1/3/4/5。
全部文件由 AIC8800D80 WiFi 传输。试验根目录、fixture 身份和数据副本边界延续上一轮。

C++ 更新器提交 `af4920c` 修正健康/回退错误原码，程序 SHA256
`46e1bce4cc46bd7a4b4c62d42ae8841a5b7bbdbed7b20e633c05b9472fa5cc4f`。
回调未启动、超时和崩溃分别返回稳定类型码，正常退出失败保留数字原码；回调输出正文不进入普通日志。
主机新增“新版本 73、回退成功”与“新版本 73、回退 74、事务保留后人工恢复”测试。
实板系统试验 hook 对序号 4 明确注入 exit73，结果 health/73、CLI exit1、回退到序号 2、health=0，
原码未被健康失败通用码覆盖。此注入只在系统试验工具中，不在应用载荷内。

序号 5 已验证持久事务为 prepared、current 仍是序号 2，系统 hook 在 stop 前提供一次受限 45 秒窗口。
复核 Workbench 作用域/空闲后，通过已保存单继电器 GPIO6 执行一次 cycle/off1000ms。
冷启动日志、boot_id 变化及后续事务/高水位证据证明物理恢复；最终 current2、health0、事务清理、
高水位5、失败版本隔离。开机早期曾观察到 health0 而事务仍存在，只有后续终态清理才计为通过。
显式重放序号5返回 verification/rejected，未启动安装；不是自动重试失败硬件动作。
至此覆盖 prepared 和 switched 两个边界，仍不是全部持久写入阶段的掉电矩阵。

在实际 framebuffer 上检查 1024×600 Qt 桌面。受限 C++ 测试工具仅绑定 gt9xxnew_ts/event4；
最初错误的 Type B/释放事件未触发完整点击，按厂商驱动的 MT Type A 协议修正后进入设置/更新页。
这是合成事件自动化，不补充人工触摸精度/亮度滑块舒适度验收，也不更改驱动/校准。
点击检查按钮产生新任务 adc492b8264349cdac957435ca507010，success、elapsed3724ms、candidate=null。
从无候选任务请求下载返回 verification/rejected/Invalid source task；没有产生假下载成功。

真实屏幕发现完成后右侧三项仍显示“检查中”。提交 `26088ef` 清理原生终态前置检查显示，
无候选显示“无匹配版本 / 无候选清单”，失败/无效任务显示可见终态；新增实际客户端进程的 Qt 回归。
完整签名序号6包安装 exit0、健康0；HMI SHA256
`26ff2284a466d935ab651ffeb4acfd2f46e2263ec500f6242d64d11799b6a0fd`，
应用包 SHA256 `760bd63a2da7660a918f785848bdcb487b3cf2a048a9f100015cc90f15863f2b`。
再次点击产生真实任务 0fbfd50d7d6747cfadcd34fb3e23e7d1，success、elapsed1810ms、无候选，
实际 framebuffer 已复核终态文字和禁用的下载/安装按钮。截图只保存本地，不上传。

prepared 恢复后，原系统恢复工具预检查拒绝了已退出的显示服务 PID。原始错误为
resource-lock/resource-busy：临时启动层固定等 1 秒仍可能与更新服务的恢复锁竞争。
只修改系统侧试验启动层，在显示服务一次启动后通过固定 display.brightness.get RPC 和拥有进程
完成有界就绪等待，再启动更新服务；没有自动重发硬件动作。再次正常物理 cycle，boot_id 变化，
序号6/应用健康0，显示RPC成功读回100，WiFi自动192.168.1.64，Web→原生亮度140→100通过，
原系统恢复工具 `--check` 回到 exit0。未执行 `--restore`，原 S70/S71 仍备份/隐藏，试验层继续运行。
系统启动工具在本地 dist/board-continue-20261014/S70native-pilot-ready，源哈希在结构化记录中；
它仍是固定本次试验目录的工具，不作为正式固件集成产物或生产重现包。

原系统/出厂身份及原用户数据哈希检查 exit0，试验数据哨兵 exit0。新增公钥只由系统试验配置选择；
两批私钥在主机签名后删除，正式信任配置位置未改动。这不构成生产密钥轮换或数据迁移验收。
旧 Python Web/监督/存储/健康工具继续作为试验依赖，原生生产替代仍未完成。

软件验证：153 项主机（5+148，含26原生）、35 项 Qt、原生/HMI S4交叉构建通过。
应用提交26088ef的 [CI](https://github.com/dshanpi/DshanControl/actions/runs/38579093670) success。
历史af4920c的Qt CSV文件选择用例失败，保留了缺陷日志；提交59e895f改为通过可见文件名编辑器键盘输入，
所有路径/导出字节/取消/失败断言保留，完整35项Qt及两个新目录独立CSV运行通过，
[59e895f CI](https://github.com/dshanpi/DshanControl/actions/runs/38581228668) completed/success。

新结构化记录：[本轮实板继续验证](validation/2026-10-14-board-native-ota-continuation.json)。
H04仍in-progress、automatic_updates_qualified=false。尚缺正式硬件修订/系统契约、系统信任根与维护互锁、
真实匹配的GitHub包下载和GUI安装、工业功能健康、生产数据迁移/权限、完整掉电矩阵和老化。

### Repository/source synchronization（2026-10-14）

用户要求全部整理并同步应上传内容。基线2b58740已与远端开发分支一致，且其
[CI](https://github.com/dshanpi/DshanControl/actions/runs/38581997406) completed/success。
本轮对齐落实方案、发布/硬件门禁、S4板卡说明和历史Python更新器说明，清除当前状态中
“仅检查”“尚未提交”等已过时描述；历史执行记录及未通过的正式发布门禁继续保留。

8份实板试验源码归档于 `boards/t113s4-sdnand/validation/20261014-native-pilot/`，
manifest记录本地来源、逐文件SHA256、用途和reference-only边界。只归档原字节并设置非执行权限，
没有接入应用载荷或正式Buildroot安装器。生命周期包含故障注入，旧Python健康/存储为冻结试验参考；
不能把这些文件当成量产更新服务、通用板卡配置或自动部署入口。

新增仓库门禁校验归档文件集合、路径、哈希、Shell语法和Python3.7语法。实测损坏源码和
未声明文件均被拒绝，随后还原并重新通过。153项主机测试（5+148，含26原生）通过；
归档C++ framebuffer/input工具使用同一S4 qmake/sysroot交叉编译退出0。
实际调用通用应用载荷组装验证11个文件，试验归档/系统运行端均未进入应用包。
本轮没有UI/运行逻辑修改，35项Qt和此前CI证据沿用对应源码，新提交还由远端CI重新检查。

固件、镜像、测试签名包、图像、原始日志、设备/用户数据和私密配置继续本地保留，
不纳入Git，不创建公开Release。本轮不烧录、不重启、不执行归档恢复/故障注入，不改设备当前第6版状态。
SDK为多仓组件工作区，应用仓库已保存适配补丁与来源记录；完整可恢复SDK输入仍是未关闭门禁，
不把上传应用源码描述成已上传全部厂商SDK或量产镜像。

### Native S4 system package integration（2026-10-15）

新 S4 Buildroot 配方在同一目标工具链中构建 HMI 与 C++ 运行端，安装系统程序、配置及
S69 启动入口，应用载荷不包含这些文件。S70 复用单个服务完成恢复后再启动应用；
原生模式下 S71 不启动旧 Python watcher。安装器的旧参数调用保持兼容，但拒绝与已有
原生目标混装；安装前先检查所有目标及父目录的符号链接，拒绝后保留旧 factory 和外部文件。

修正服务在验证持久存储前创建任务目录的问题。新增服务所有者关停 RPC：取得任务、
更新、控制三把锁后才接受关停，保持锁至退出并拒绝新请求；占用时保留服务，返回
原始 resource-busy。启动器核对 PID 的 executable 和参数，通过真实 identity RPC 确认就绪，
一次调用只启动一次；拒绝误停复用 PID，结构化启动错误保留在 0600 的本地日志中。

158 项主机测试（5+153，含 29 项原生）、35 项 Qt 测试通过；目标原生程序交叉构建通过。
真实 S4 配方的 configure/build/install 宏在隔离目录执行成功，重编译 HMI 与原生程序，
144 项实际源码输入逐项核对；两个 ARM32 EABI5 softfp ELF 的 24 项动态库依赖在固定
sysroot 中找到并记录哈希。此排练复用原已固定的目标依赖，不等于全 SDK/rootfs 干净重建。
init 的进程/启动/占用/失败测试在主机执行，不是 BusyBox 实板启动通过。

硬件 MCP 在本次连接检查中拒绝 127.0.0.1:38928，未取得实时板卡身份，未操作硬件。
正式身份/公钥、维护互锁、最小权限、工业健康、GitHub 下载安装和完整掉电验收均未关闭；
不改旧镜像/第6版试验的历史证据，不生成公开 Release。完整输入和哈希见
[系统接入记录](validation/2026-10-15-native-system-integration.json)。

完整新构建前的启动复核发现：替换厂商 S60 后，其创建 `/dev/by-name` 的步骤也消失，
原生存储约束却需要 UDISK 别名。S69 现仅调用厂商 `do_device_node_switch`，随后验证
别名/启动映射/实际块设备，不调用格式化函数；配方显式依赖 busybox-init-base-files。
158 项主机测试重新通过。真实厂商函数在隔离的 `/dev` 和启动参数夹具中执行通过，
合成设备内容不变；缺失函数时启动在初始化/创建进程前拒绝。此证据不是实际硬件挂载。
新 rootfs 构建在应用包尚未构建时暂停，更新冻结源码后继续，保留本轮依赖编译结果及中断记录。

恢复路径复核进一步修正启动等待：原 30 秒循环短于允许的回调/健康观察。
就绪等待现按 `/proc/uptime` 有界计算，覆盖原生配置上限；通过服务所有者专用的
`service.ready` 核对实际 daemon PID，拒绝其他实例和错误类型/额外字段，识别退出/僵尸进程。
BusyBox fragment 显式启用所需 start-stop-daemon、fancy PID、readlink 和 tr。
159 项主机测试（5+154，含30原生）通过，原生 ARM 重新交叉构建通过。
真实 C++ 进程在 prepared 恢复中执行一次 31 秒 stop hook，事务清理、失败隔离后才报告就绪；
最终 init 单项再次通过。主机夹具验证不能替代目标 BusyBox 冷启动/物理掉电测试。

2026-10-15 干净 ext4 构建的启动入口补全：厂商 MMC 环境明确 `init=/init`，
但直接 Buildroot ext4 只安装 `/sbin/init`；旧 target 遗留的 BusyBox 链接不能作为
构建输入。正式 post-build 现验证目标 BusyBox 后建立 `/init -> bin/busybox`，遇到
异物、错误链接或 BusyBox 路径链接先拒绝，完整镜像预检同步验证。161 项主机
测试通过（含18项 S4 系统测试）；C++/Qt 源码未变，实板启动仍未验证。详见同一
[系统接入记录](validation/2026-10-15-native-system-integration.json) 的 boot_entry_followup。
