# 编译、测试并扩展应用

**执行位置：** Ubuntu 开发主机。先完成[快速开始](quickstart.md)，确认使用主机 Qt5 和独立构建目录。

## 选择正确的检查

| 命令 | 覆盖内容 |
| --- | --- |
| `make test-bootstrap` | 建立 Python 测试虚拟环境，按锁定要求安装依赖 |
| `make test` | 原生运行端、桌面/媒体构建，核心与仓库回归及静态门禁 |
| `make test-hmi` | Qt UI、隔离、上报、注册器及原生更新桥/流程测试 |
| `make test-application-registry` | 注册 ID 与上下文校验的定向检查 |
| `make build-desktop` | 构建桌面服务、模型测试与媒体进程 |

测试清单以 [Makefile](../../Makefile) 为准。改动前后记录提交、Qt/Python 版本、命令、退出码、失败和 skip；不能把历史结果移到新提交名下。

## 推荐开发循环

1. 在干净分支记录基线，阅读 `AGENTS.md` 与相关设计合同。
2. 修改明确功能，保持现有路径、数据格式和业务门禁。
3. 用针对性测试快速定位错误，再运行仓库要求的 `make test` 和 `make test-hmi`。
4. UI 有变化时重新生成真实源码渲染并检查窄/宽布局、不可用态与错误态。
5. 需要板端交付时再用匹配 SDK 交叉构建，单独完成硬件验证。

## 添加一个应用入口

注册文件位于 `components/hmi/src/desktop/assets/app-registry.json`。它是编译进资源的元数据，不是可执行插件清单。

1. 添加稳定 ID、标题、图标、颜色、分类与唯一顺序。
2. 在 `applicationregistry.h` 增加明确的页面处理器和上下文约束。
3. 在 `MainWindow::openApplication()` 接入实际页面及后台能力。
4. 测试有效路由、重复 ID、重复顺序、未知字段和越界上下文。
5. 检查进入/返回工作台的状态，确认失败时原页面不变。

仅添加一条 JSON 不会自动启动外部程序。后台权限检查也不会由注册成功获得。

## 生成和审查截图

```bash
mkdir -p dist/hmi
cd dist/hmi
/usr/bin/qmake ../../components/hmi/src/omnigate-hmi.pro
make -j2
QT_QPA_PLATFORM=offscreen ./omnigate-hmi --desktop-render ../desktop-render
```

以新目录保留本次输出，核对工作台、抽屉、编辑、图片与宽屏。批准参考图和原始需求不可改写来隐藏差异。`--desktop-capture` 是窗口抓取入口，和固定 fixture 的含义不同。

## 打包源码预览

提交完并保证 tracked 树干净后，在根目录运行：

```bash
make package RELEASE_DIR=dist/source-preview
```

工具只导出已提交 Git 对象，生成 SHA256 和源提交说明；未跟踪文件不进入归档，已有同名输出拒绝覆盖。这不能替代应用包或系统固件构建。

## 常见测试边界

权限相关用例可能因环境跳过；用例数量与需求验收数量不同。Unix socket 临时目录尽量短。Qt offscreen 测试不证明物理触摸；模拟 TLS/MQTT 测试不证明外部生产账号已开通。

下一步：[本机接口](api.md)或[目标板构建](board-porting.md)。原始工程要求：[AGENTS](../../AGENTS.md)。
