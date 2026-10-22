# DshanControl

**面向带屏工业网关与边缘设备的 Qt5 组件桌面和应用框架。**

把设备趋势、告警、媒体与业务入口放在同一个工作台，通过本机 API 和独立后台服务连接设备与外部业务。

> 当前为源码开发预览。源码合并、主机渲染与板端安装分别核验；本文图片均为当前源码的主机固定示例，不是当前板端截图。生产资格与未验收项见下方状态。

![DshanControl Qt5 组件工作台](docs/images/qt5-desktop/workbench.png)

## 工作台与应用框架

- **可配置工作台**：组件添加、编辑、删除与布局保存，组合设备趋势、图片、媒体、云、家庭和语音入口。
- **统一应用入口**：12 个稳定应用 ID，集中管理注册、分类、路由和上下文校验；无效参数在切页前拒绝。
- **桌面交互**：顶部控制中心、底部应用抽屉、中文搜索与拼音输入，应用容器支持进入、返回与遮罩状态。
- **后台与系统分层**：界面通过本机 API / IPC 获取能力，后台承担协议和业务边界，系统管理进程角色、健康、维护许可与更新事务。

本项目提供可扩展框架与业务界面；源码中的适配入口不等于现场设备、外部账号或硬件功能已经完成验收。

## 新版页面

以下图片均由当前源码在 Ubuntu / Qt5 主机上实际渲染，使用固定示例数据。10 张批准图的完整 RGBA 比较差异为零；不替代物理 LCD、触摸和真实业务验证。

<table>
  <tr>
    <td width="50%"><strong>顶部控制中心</strong><br><a href="docs/images/qt5-desktop/control-center.png"><img src="docs/images/qt5-desktop/control-center.png" alt="Qt5 顶部控制中心，主机示例渲染" width="100%"></a></td>
    <td width="50%"><strong>全部应用</strong><br><a href="docs/images/qt5-desktop/app-drawer.png"><img src="docs/images/qt5-desktop/app-drawer.png" alt="Qt5 全部应用抽屉，主机示例渲染" width="100%"></a></td>
  </tr>
  <tr>
    <td><strong>编辑工作台</strong><br><a href="docs/images/qt5-desktop/widget-editor.png"><img src="docs/images/qt5-desktop/widget-editor.png" alt="Qt5 组件编辑，主机示例渲染" width="100%"></a></td>
    <td><strong>组件库</strong><br><a href="docs/images/qt5-desktop/widget-library.png"><img src="docs/images/qt5-desktop/widget-library.png" alt="Qt5 组件库，主机示例渲染" width="100%"></a></td>
  </tr>
  <tr>
    <td><strong>应用搜索</strong><br><a href="docs/images/qt5-desktop/app-search.png"><img src="docs/images/qt5-desktop/app-search.png" alt="Qt5 应用搜索，主机示例渲染" width="100%"></a></td>
    <td><strong>图片组件</strong><br><a href="docs/images/qt5-desktop/image-widget.png"><img src="docs/images/qt5-desktop/image-widget.png" alt="Qt5 图片组件，主机示例渲染" width="100%"></a></td>
  </tr>
</table>

[展开组件](docs/images/qt5-desktop/expanded-widget.png) · [1280×720 宽屏示例](docs/images/qt5-desktop/wide-screen.png) · [原始批准设计与需求](docs/design/component-desktop-v1/README.md)

## 架构

![DshanControl Qt5 应用框架架构](docs/images/qt5-desktop/architecture.svg)

应用通过 `DesktopShell → ApplicationRegistry → 业务页面` 进入。后台服务分别处理设备数据、云与家庭/语音业务、媒体会话；Native Runtime 管理进程生命周期、权限和更新事务。真实现场维护许可属于系统边界，不能由界面状态或实验室钩子替代。

布局、私密配置和业务数据独立保存，不进入公开源码包。架构图描述源码组织，**不表示当前板端已经部署全部角色和服务**。

- [应用注册、路由与扩展方法](components/hmi/src/desktop/README.md)
- [架构与调用边界说明](docs/architecture/README.md)
- [完整源码预览与验证范围](PREVIEW.md)

## 主机开发

当前适配目标为 **T113s4 SD NAND / Linux / Qt5.11.3**；目标板资格仍待完成。主机开发与本次验证使用 Ubuntu / Qt5.12.8；目标 SDK、Cedar 和 Qt 私有 ABI 组件须保持匹配。

主机需要 C++ 工具链、Qt5 Core/Gui/Widgets/Network/Test 与 qmake，以及 OpenSSL、zlib、SQLite 开发包和 Python3 venv。私有 TLS/MQTT 回归另需 Mosquitto 及客户端工具。

```sh
git clone https://github.com/dshanpi/DshanControl.git
cd DshanControl
make test-bootstrap
make test HMI_QMAKE=/usr/bin/qmake
make test-hmi HMI_QMAKE=/usr/bin/qmake
```

构建并生成独立示例图片：

```sh
mkdir -p dist/hmi
cd dist/hmi
/usr/bin/qmake ../../components/hmi/src/omnigate-hmi.pro
make
QT_QPA_PLATFORM=offscreen ./omnigate-hmi --desktop-render ../desktop-render
```

主机窗口预览使用 `./omnigate-hmi --windowed`。实时界面需要本机 Web API 和后台 IPC；后台不可用时保留不可用状态，不回填示例数据。固定渲染用于视觉回归，不会连接真实业务。

## 代码导航

|目录|职责|
|---|---|
|`components/hmi/src/desktop/`|工作台、应用注册、组件编辑、抽屉、布局、图片与输入|
|`components/hmi/src/`|设备、总线、网络、告警、云、家庭/语音、媒体及更新界面|
|`components/desktop/`|本机 IPC 与云、上报、家庭、语音服务边界|
|`components/runtime/`|原生进程、角色权限、健康、更新事务和回退|
|`components/media/`|媒体工作进程、表面租约及目标解码/显示适配|
|`components/core/`、`components/web/`|统一模型、业务 API 与 Web 服务|
|`boards/t113s4-sdnand/`|目标系统集成、配置与契约|
|`tests/`、`scripts/`|主机回归、构建、源码与目标包校验|
|`docs/design/component-desktop-v1/`|批准设计、交互合同和原始验收要求|

## 验证与当前状态

|检查|本次实际结果|范围|
|---|---|---|
|核心与仓库主机回归|5 项核心；324 项仓库用例，11 项因权限环境跳过；无失败|最终提交的 Ubuntu 主机复核|
|Qt 界面回归|97 主回归 + 6 隔离 + 15 上报页面 + 7 注册器用例；无失败|主机 Qt5，非物理交互|
|原生更新边界|3 个 RPC 错误投影、2 个私有下载流程模式通过|隔离合成任务，非现场安装|
|批准图比较|10 张完整 RGBA 逐像素差异为零|固定示例，原图不变|
|GitHub 自动检查|[通过（查看记录）](https://github.com/dshanpi/DshanControl/actions/runs/39793430696)|独立远端构建与回归|
|原始需求验收|**25 PASS / 48 PARTIAL / 0 RUNNING**|共 73 项，仍未全部完成|

Qt 用例数量不等于需求验收数量。[73 项记录](docs/validation/component-desktop-preview-acceptance.csv)与[源码框架验证](docs/validation/2026-10-22-framework-source-validation.json)保留实际范围和跳过项。

仍待完成真实云/家庭/语音/现场总线、物理 LCD/触摸/音频、完整媒体资源回收、原生角色跨启动、持久安装与回退，以及独立真实 86400 秒测试。历史中断时段不能合并为 24 小时通过；当前没有已确认运行的独立长测。

## 源码交付与板端安装

`make package RELEASE_DIR=dist/source-preview` 只从干净、已提交的 Git 树导出源码，生成 SHA256 与源提交说明。未跟踪文件不进入归档；存在未提交改动或同名输出时拒绝导出。

**源码包不是可安装应用包或系统固件。** 源码推送、合并和发布不会让板端自动变成新版 UI。此前临时运行覆盖在重启后失效；本轮没有安装或烧录新版。

现场仍连接设备时，部署前必须取得真实、适用的维护互锁许可，并满足系统契约、目标 ABI、受信签名、健康与保数据回退门禁。当前互锁尚未接入，不能用实验室脚本、空回调或开发标志绕过。[正式发布规则](docs/RELEASING.md)

## 许可与资料

项目许可见 [LICENSE](LICENSE)，第三方来源与许可见 [THIRD_PARTY.md](THIRD_PARTY.md) 及各组件目录。源码交付不包含设备密钥、用户数据、私密配置、私有 SDK 或厂商预编译库。

历史系统交付和实板证据保留在 `docs/` 中，作为有边界的历史记录；首页展示以本次 Qt5 组件桌面为准。
