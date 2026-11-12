# 建立编译、测试与源码交付流程

在 Ubuntu 开发主机上建立可重复的修改流程。前提是已完成[快速开始](quickstart.md)，能够使用主机 Qt5 编译 HMI；改动前阅读仓库 [AGENTS.md](../../AGENTS.md) 和对应模块合同。

## 1. 建立开发分支并记录基线

在**仓库根目录**检查工作区，确认已有修改已妥善处理后创建分支。分支名按实际任务替换：

```bash
git status --short
git switch -c work/your-change
git rev-parse HEAD
/usr/bin/qmake -v
python3 --version
```

记录提交、操作系统、编译器和 Qt/Python 版本。主机测试与目标板构建使用不同工具链和输出目录。

## 2. 初始化测试环境

在**仓库根目录**执行：

```bash
make test-bootstrap
```

该目标创建 `.test-venv` 并安装测试要求。成功判据是命令退出码为 0，虚拟环境解释器可以运行。依赖没有变化时无需每次重新初始化。

## 3. 修改并运行定向检查

| 修改范围 | 可先执行的命令 |
| --- | --- |
| 应用注册、上下文路由 | `make test-application-registry HMI_QMAKE=/usr/bin/qmake` |
| 桌面后台与模型 | `make test-desktop HMI_QMAKE=/usr/bin/qmake` |
| 原生运行端 | `make test-native HMI_QMAKE=/usr/bin/qmake` |
| HMI 页面与交互 | `make test-hmi HMI_QMAKE=/usr/bin/qmake` |

失败时先定位与本次修改相关的第一条失败，修复后复验。定向检查用于缩短调试周期，不替代仓库要求的完整检查。

## 4. 运行工程门禁并保留日志

以下在 **Bash、仓库根目录**执行。`pipefail` 用于保留测试命令失败状态，避免只得到 `tee` 的成功退出码：

```bash
CHECK_DIR="dist/check-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$CHECK_DIR"
set -o pipefail
make test HMI_QMAKE=/usr/bin/qmake 2>&1 | tee "$CHECK_DIR/host.log"
```

确认退出码为 0，并检查失败与跳过，再执行：

```bash
make test-hmi HMI_QMAKE=/usr/bin/qmake 2>&1 | tee "$CHECK_DIR/hmi.log"
git diff --check
```

`make test` 包含原生端、桌面/媒体构建及核心、仓库回归；`make test-hmi` 包含注册器、Qt 交互、隔离、上报和原生更新桥/流程。权限或角色条件可能使部分用例跳过，须记录数量与原因。

## 5. 审查界面变化

重新编译 HMI，并使用新的输出目录生成固定示例：

```bash
mkdir -p dist/hmi
cd dist/hmi
/usr/bin/qmake ../../components/hmi/src/omnigate-hmi.pro
make -j2
RENDER_DIR="../render-$(date -u +%Y%m%dT%H%M%SZ)"
QT_QPA_PLATFORM=offscreen ./omnigate-hmi --desktop-render "$RENDER_DIR"
cd ../..
```

逐图检查中文、组件、遮挡、宽屏和错误/空状态。需要验证真实交互时使用窗口流程；`--desktop-capture` 是运行窗口抓取，与固定示例渲染含义不同。

保留批准参考图与原始需求，不通过修改参考图隐藏实现差异。

## 6. 增加应用入口

1. 在 `components/hmi/src/desktop/assets/app-registry.json` 添加稳定 ID、显示标题、图标、颜色、分类和唯一顺序。
2. 在 `applicationregistry.h` 增加明确处理器及允许的上下文。
3. 在 `MainWindow::openApplication()` 接入实际页面和后台能力。
4. 检查重复 ID/顺序、未知字段、错误上下文、成功进入与返回。
5. 确认路由失败时原页面不变，后台仍执行独立权限校验。

注册文件是编译进资源的元数据，不是任意程序启动清单。只增加 JSON 条目不会自动产生页面或外部服务。

## 7. 导出源码预览

完成检查并提交所有应交付的文件后，确认 tracked 工作树干净。在**仓库根目录**使用新的输出目录：

```bash
git status --short
make package RELEASE_DIR="dist/source-preview-$(date -u +%Y%m%dT%H%M%SZ)"
```

打包工具只导出已提交 Git 对象，生成 SHA256 和源提交说明；未提交改动会拒绝，未跟踪/忽略文件不进入归档，已有同名归档不覆盖。

该结果用于源码交付。应用包与系统镜像分别按[更新与发布要求](updates.md)和[S4 构建流程](board-porting.md)处理。

技术依据：[Makefile](../../Makefile)、[回归入口](../../scripts/test_all.sh)、[源码打包工具](../../scripts/package_release.sh)。
