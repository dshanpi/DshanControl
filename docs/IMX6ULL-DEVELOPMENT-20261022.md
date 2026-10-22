# 6ULL Pro Lite 开发移植与验收

本分支基于 `feat/qt5-component-desktop-preview` 的
`0584604f5c038fb21040a1cd6c90ab2f5b5e847a`，保留现有 T153/S4 实现，新增
[板级接入](../boards/imx6ull-pro/README.md)。本记录是开发范围验收，未取得正式系统
支持、生产安全或签名 OTA 资格。

## 目录与构建入口

- `boards/imx6ull-pro/`：SDK 输入、Buildroot 外部包、Linux 补丁、系统配置、
  NXP UUU 脚本、镜像校验和板端读回工具。
- `components/media/src/gstworker.h`：可选 GStreamer 软件视频后端；其他目标仍可
  使用原有后端，不将 PxP 或软件解码声称为硬件视频解码。
- `components/hmi/src/mediasession.*`、`mediapanel.cpp`：有界私有帧通道及 Qt 渲染。
- `tests/test_imx6ull_integration.py`：来源提交/补丁、错误输入和镜像布局回归，
  纳入 `make test`；Qt 帧通道用例纳入 `make test-hmi`。
- `platform/boards/imx6ull-pro.json`：开发候选登记，正式支持列表为空。

SDK 固定为 `100askTeam/aibsp-imx6ull-pro_linux5.4.47` 的
`e00afed02481332d2de44a498518b596651dcdef`。内核和 U-Boot 子模块、工具链、
Qt 版本见板目录 `source-identity.json`。完整 SDK、构建缓存、厂商固件、测试视频、
镜像、Wi-Fi 凭据、私钥和设备用户数据不进入应用 Git。

从 `6ull` 分支构建无需再次应用历史补丁。`build.sh` 生成实际 HEAD/dirty 来源记录，
将其传给 Buildroot 和安装器。历史模板的基础提交及补丁哈希与新构建的提交身份分别
保存，不能将旧镜像的实板结果当成本分支后续构建的验收。

## 已验证的历史视频完整镜像

目标为 100ASK i.MX6ULL Pro、512 MiB、eMMC、7 寸 RGB 1024×600，Linux 5.4.47、
Qt 5.15.14、ARMv7 hard-float / glibc 2.41。实际 PCB 丝印修订仍未取得。

| 项目 | 实际证据与范围 |
| --- | --- |
| 应用来源 | 上述 preview 基础提交加补丁 `8e4504b20eb6d0d054987bdd64a543a031ce4e0b690049686054ea4279dedf0d` |
| 全盘镜像 | `sdcard.img` SHA256 `c617cc9e42487c72d2eb7f68db4331eb2937e3697f0a802366af7a21a9be6bfe` |
| LYNX 写入 | `flash-1792645539895559700`，NXP/UUU，success / exitCode 0；任务成功后单独重新上电 |
| 读回 | 25 项通过，包括引导程序、完整 boot FAT、四个应用文件、来源身份和 GUI 心跳；未声称启动后 root/data 全盘逐字节相等 |
| 屏幕/触摸 | 用户在本次完整镜像上确认“屏幕和视频正常，暂停/继续触摸准确”；被动采集窗口内零事件，不把用户确认记为原始事件证据 |
| Wi-Fi | 用户私有配置恢复后正常重启自动关联，网关、两个 DNS 和两个校验证书的 HTTPS 目标通过；原账号 LAN 检查 7 项通过 |
| 用户数据 | 两份数据库副本完整性通过，恢复前核对 12 个非 SHM 文件；重启后 5 份静态配置及两段媒体哈希匹配，亮度 156 |
| 视频控制 | 11 项真实 GUI 合成触摸检查通过，含暂停、拖动进度、继续、隐藏/恢复、全屏和切换本地视频；另有上述现场手指操作确认 |

源视频是生成的 20 秒 H.264 Baseline / yuv420p / MP4，15fps，无音频。
软件帧上限 320×240，按原比例输出，再由 Qt 放大显示。单核 792MHz、每模式约
8.5 秒采样：

| 源视频与模式 | 内部输出 | 帧交付率 | HMI CPU | worker CPU |
| --- | --- | ---: | ---: | ---: |
| 640×360 窗口 | 308×174 | 14.53fps | 25.2% | 45.8% |
| 640×360 全屏 | 320×180 | 13.65fps | 42.1% | 39.9% |
| 320×240 全屏 | 320×240 | 14.27fps | 47.2% | 30.4% |

帧率由 worker 交付计数计算，不是光学/VSync 测量；全屏放大不会增加细节，不推断
高清或任意编码性能。音频按用户要求未启用，未进行长时间满负载测试。

历史适配阶段主机主套件共 289 项，279 通过、10 跳过，HmiUiTest 90 项通过；
专用主机与 ARM 视频 worker 测试通过。这些计数不代表本分支新增提交的 CI 结果。
新的来源/镜像回归与三套 Qt 测试由分支工作流运行，不把跳过的权限测试记作本板通过。

## 仍未完成的门禁

`hardware_revision=pending`、`validation=not-run` 保持其正式资格原义。
`development_mode=false`、`automatic_install=false`；设备未预置生产更新公钥，
也未提供本板维护和 stop/start/health 钩子，不能签名升级或自动放行安装。
原生 runtime 启动会检查恢复事务，但没有本板真实更新中断、回退和数据闭环证据。

当前应用以 root 运行，受管降权、最小权限、原生监督和工业维护互锁尚未集成。
应用打包器对私有图像插件的强绑定尚需适配本板 Qt 标准插件方案。正式发布还需要
干净 SDK/完整来源交付、运行库/契约锁、签名包、错配/篡改/锁冲突拒绝、升级阶段
掉电与健康失败回退、GitHub 附件回下载及首次基线闭环。

Buildroot legal-info 已收集包清单与许可证，但保留 Buildroot 源码未保存、外部
工具链许可文件未收齐的警告；源码分支不等于完整 SDK/许可交付闭合。
以太网本轮未接入链路；CAN/RS485/EtherCAT 从站、语音、云服务与生产安全均不宣称
已验证。新增分支不修改原 S4 的资格结论，不发布 Release 或更新现有不可变附件。

[结构化记录](validation/2026-10-22-imx6ull-development.json) 保存输入和已完成范围；
验收仅对应其中固定镜像。分支提交时的来源记录/候选状态整理没有重新烧录到板端。


## 分支提交前的本地主机门禁

完整 `make test`：core 5 项通过，主套件 299 项（289 通过、10 项因真实 root/角色
条件跳过）；`make test-hmi` 三套分别 90、6、15 项通过。新增软件媒体门禁收到
60 帧、34 个不同帧哈希，包含故意无效输入的拒绝检查。来源与镜像回归 10 项通过，
暂存区空白、私密凭据、二进制载荷及旧板目录范围检查通过。以上为本地结果，远端
CI 以对应提交的 Actions 为准；这些检查不提高本板 OTA 或生产资格。
