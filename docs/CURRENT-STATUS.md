# 当前交付与源码入口

记录日期：2026-10-31。本索引用于区分公开源码、已安装版本与尚未上板的候选；历史记录保留各自的验证范围。

## 从哪里开始

- 最新功能工作分支：fix/qt-application-workflows-20261028；尚未合并 main。
- [24 页交付与功能变化](design/page-gates/implementation-delivery-dp.md)，各页独立记录位于 [page-gates](design/page-gates/manifest.json)。
- [页面设计、实现与交付门禁](design/page-delivery-gate.md)。
- [S4 系统集成与 dnsmasq 回归方法](../boards/t113s4-sdnand/README.md)。
- [源码打包与开发环境](../PREVIEW.md)、[正式发布规则](RELEASING.md)。

## 源码与板端对应关系

|范围|功能源码提交|实际状态|
|---|---|---|
|DP 页面与公共界面|965087a4cfd878e078e29cb1ce65cd4a7e2a4a44|完整开发试验镜像已烧写、校验并启动；后续只读身份仍对应此版本|
|DQ 网络服务兼容修复|f91a5eb00b3e0cf3bc332768676b54c9e2d73552|源码已推送；ARM 候选已构建和仿真验证，尚未安装|
|DR 说明整理|621b5cc6bb03d5c38d5be0574d016ea6e6fe9126|更新说明与入口，功能源码不变|
|DS 网络服务启停|见 [DS 验证记录](validation/2026-10-31-dnsmasq-init-results.json)中的源码摘要|失败退出码传递与停止失败后的重启阻止已验证；尚未安装|

DP 镜像 SHA256：8422bc3c62375682b84ef930d1fe96797d3dfecbb4bd751a790b0b1f4ecb6b06。
实际 HMI SHA256：375459e108aa3040022ba5093b1f0fac47db94863ee8d8285b0264e2001e315e。
烧写日志保留末尾第二次重启的 Device not found 文本；最终任务成功与独立 Linux 启动核验分别记录。帧缓冲截图不证明物理屏幕发光；本版物理 LCD 与手指导航仍未确认。

DQ 使用 dnsmasq 上游 NO_INOTIFY 编译选项，适配未启用 inotify 的 S4 内核。它保留普通解析文件轮询和 hosts 重新加载；目录监听配置明确拒绝。该修复不等于 Wi-Fi 联网、AP/DHCP、DNS/NTP 或外部业务已恢复。

## 验证范围

|记录|结果|边界|
|---|---|---|
|DP HMI|注册 7、主回归 121、隔离 6、上报 15；全部通过|Qt5 主机行为与合成场景；非全部现场业务|
|DQ 核心及仓库|核心 5 项通过；仓库 394 项中 383 通过、11 项权限环境跳过、0 失败|运行时设置实际 dnsmasq 测试程序，4 个新增兼容用例确实执行|
|DQ 二进制对照|旧程序 4 项失败、新候选 4 项通过；主机与 ARM 用户态仿真分别验证|实际 DNS UDP/TCP、解析文件轮询、hosts 重载和目录配置拒绝；不是实板网络验收|
|原始 73 项|25 PASS、48 PARTIAL、0 RUNNING|无独立 86400 秒通过记录|

DQ 未改 HMI，未重复 DP 的界面矩阵；最后全量回归后仅追加文档与账本，运行代码和测试源码保持一致。详见 [DQ 验证记录](validation/2026-10-31-dnsmasq-compatibility.json)及 [73 项验收表](validation/component-desktop-preview-acceptance.csv)。

DP 原始烧写、身份和帧缓冲证据留在 SDK 交付目录“逐页图稿对照收束_DP”；DQ 原始日志留在“网络服务兼容验证_DQ”。公开仓库只含通用记录与证据引用，不包含设备配置、私密材料、SDK 或生成的程序与固件。

## DS 服务启停补充验证

DS 修复继承启动脚本“打印 FAIL 却返回成功”的问题，并阻止停止失败后的再次启动。主机命令/安装检查 9 项、真实程序启停 5 项、S4 集成 12 项通过；目标 BusyBox shell 仿真下的 8 项命令检查也通过。未重复无变化的界面和后端矩阵，未把退出码当成就绪证明。详见 [DS 记录](validation/2026-10-31-dnsmasq-init-results.json)。

实板只读观察已提交一次，结果读取因共享硬件操作忙而暂缓；命令不重发，候选未安装。

## 仍需完成

真实网络、现场总线、云/家庭/语音、媒体和物理交互，以及资源生命周期、持久回退、真实切页与独立长测，继续按各自门禁验收。当前系统契约和生产资格保持未通过状态。

首页 PNG 是历史框架固定示例；2026-10-22 的零像素差异只适用于[当时的验证](validation/2026-10-22-framework-source-validation.json)。后续页面状态、功能变更和主机截图以各页 history 中的实际记录为准。
