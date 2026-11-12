# 理解系统分层与源码目录

DshanControl 把桌面交互、业务适配、媒体会话和系统管理分开。阅读代码时沿着一次具体动作查调用链，比先遍历所有文件更容易理解。

![Qt5 应用框架架构图](../images/qt5-desktop/architecture.svg)

## 从点击到实际能力

1. `DesktopShell` 管理工作台、抽屉、遮罩与应用容器。
2. `ApplicationRegistry` 校验稳定应用 ID 和上下文，生成明确路由。
3. `MainWindow` 或专用 panel 展示业务页，发出 HTTP/本机 IPC 请求。
4. Web API、desktop service 或 media worker 执行业务，并返回真实状态。
5. Native Runtime 在系统边界管理角色、权限、健康和更新事务。

成功切页只证明路由通过，不能证明后台授权、文件可读或现场控制成功。

## 目录导航

| 路径 | 主要职责 | 常见修改任务 |
| --- | --- | --- |
| `components/hmi/src/desktop/` | 组件、布局、注册器、键盘、图片 | 增加组件或应用入口 |
| `components/hmi/src/` | 各业务页、更新、媒体会话 | 表单、状态展示和操作流程 |
| `components/desktop/` | 本机 IPC 与业务 provider | 云/家庭/语音/传感器接入 |
| `components/web/`、`components/core/` | HTTP API、统一模型 | 数据查询、协议动作和审计 |
| `components/runtime/` | C++ 系统运行端 | 角色、健康、安装与回退 |
| `components/media/` | 独立媒体进程与适配 | 解码、表面租约和资源释放 |
| `boards/t113s4-sdnand/` | S4 系统集成 | Buildroot 配方、启动、系统契约 |
| `tests/`、`scripts/` | 回归、构建和发布工具 | 新行为的验证和来源绑定 |
| `docs/design/`、`docs/validation/` | 原始需求与证据 | 核对验收边界，保留历史失败 |

## 哪些修改属于系统层

内核、DTB、bootloader、目标 libc/Qt 基础库、公钥和系统服务契约属于系统交付。应用更新不携带这些内容。界面调整、业务代码和应用资源可以走应用线，但仍需与系统契约兼容。

S4 BSP 来自独立硬件仓库，本仓库提供用户态集成和适配。EinkOS 是文档/设计参考，不能把它的私有 SDK 或板型配置拷贝过来充当 S4 底座。

## 以一次上报为例

页面调整开关 → `reporting.configure` → 后台校验允许周期 → 可选持久配置 → MQTT 发送 → broker 确认 → 页面更新“最近代理确认”。其中页面状态、持久保存、代理确认和服务端入库各有独立检查点。

下一步：[搭建开发循环](development.md)。深入资料：[运行端 README](../../components/runtime/README.md)、[架构索引](../architecture/README.md)。
