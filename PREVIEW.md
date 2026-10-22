# Qt5 组件桌面与应用框架源码测试版（2026-10-22）

**这是源码预览，不是可安装应用包、固件或生产版本。推送源码不会更新板端界面。**

实现组件工作台、上下遮罩、应用抽屉、编辑与布局持久化、中文搜索/软键盘、趋势/图片，以及云、家庭、语音、媒体与更新业务页面。具体后台能力与硬件完成程度仍以验收记录为准。

本次框架新增统一应用注册校验、稳定 ID 路由、按元数据分类、严格上下文检查，以及应用容器当前入口状态。无效上下文在切页前拒绝。源码打包改为导出干净的已提交 Git 树，排除未跟踪文件并绑定源提交。

## 入口

- [应用框架与扩展方法](components/hmi/src/desktop/README.md)
- [原始设计需求](docs/design/component-desktop-v1/README.md)：批准图和原始合同保持。
- [73项验收进度](docs/validation/component-desktop-preview-acceptance.csv)：25 PASS、48 PARTIAL、0 RUNNING。Qt测试数量不等于需求通过数量。
- `components/hmi/src/desktop/`：桌面、注册、组件编辑、抽屉、输入。
- `components/desktop/`：后台 IPC、云/家庭/语音边界；私密配置不随源码交付。
- `components/runtime/`、`components/media/`：进程角色、权限与媒体生命周期。
- `tests/hmi/`、`tests/test_desktop_service.py`、`tests/test_desktop_voice_lifecycle.py`：界面与服务测试。

## 验证边界

本次验证结果另见 [源码框架验证记录](docs/validation/2026-10-22-framework-source-validation.json)。新增框架只按该记录声明实际完成的主机检查，不沿用旧 ARM 结果当作新源码已上板。

历史 r23/r27 候选已做过有界 ARM Qt5.11.3 回归、固定场景批准图比较，以及短时 LinuxFB/原生后台交互；这些证据不代表新框架持久安装、物理 LCD/手指/音频或真实外部业务验收。此前临时运行覆盖在重启后失效，最后直接观测仍是 factory 旧 UI。发布期间没有安装、停启板端程序或刷机。

旧长测中1000次切页与约55809秒观察因启动变化中断，不能算86400秒，更不能把不同启动或进程时段拼成24小时。当前没有已确认运行的独立长测。

仍待验收：真实云/家庭/语音/现场总线，物理交互与音频，完整媒体资源回收，原生后台跨启动、匹配系统/应用持久交付及回退、真实独立86400秒。设备仍连接现场设备，真实维护互锁未接入；实验室维护脚本不适用，源码预览不绕过此门禁。

## Ubuntu 主机验证

需要 Qt5开发包（Core/Gui/Widgets/Network/Test）、qmake、C++编译器、OpenSSL/zlib开发包、Python3。Python依赖见 `tests/requirements.txt`。私有TLS/MQTT测试需要 openssl、mosquitto、mosquitto_pub、mosquitto_sub；缺少依赖或权限的测试必须明确记录跳过。Unix socket测试请使用短临时目录。

```sh
make test-bootstrap
make test HMI_QMAKE=/usr/bin/qmake
make test-hmi HMI_QMAKE=/usr/bin/qmake
```

构建和隔离渲染见应用框架说明。正常窗口需要本机 Web API/后台 IPC；不配置外部账号时不能把不可用界面当真实业务已接入。目标板为 T113s4 SD NAND/Qt5.11.3，需要匹配、合法 SDK 和系统契约；Cedar 与 Qt私有ABI组件不能替换通用PC系统库。

`make package RELEASE_DIR=dist/source-preview` 只导出已提交源码，生成归档、SHA256和源提交说明；遇到未提交改动或已有同名输出会拒绝。系统/应用发布仍须遵守 [正式发布门禁](docs/RELEASING.md)，本次源码标签和附件不可交给板端更新器。

不包含设备密钥、用户数据、私密恢复记录、私有 SDK 或厂商预编译库；公开第三方源码和许可证随组件保留。
