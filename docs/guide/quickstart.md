# 在 Ubuntu 上完成首次体验

**适用对象：** 第一次运行 DshanControl 的用户和开发者。**执行位置：** Ubuntu 主机。此流程不需要开发板、云账号或现场设备。

## 1. 安装主机依赖

Qt 使用主机的 Qt5；不要把目标板 SDK 的 Qt 库复制到电脑系统目录。Ubuntu 22.04/24.04 可安装以下包：

```bash
sudo apt-get update
sudo apt-get install -y build-essential git qtbase5-dev libqt5charts5-dev \
  libssl-dev zlib1g-dev libsqlite3-dev python3-venv python3-pil \
  mosquitto mosquitto-clients
qmake -v
```

`qmake -v` 应显示 Qt 5.x。Python 虚拟环境和 MQTT 工具用于后面的仓库回归；只查看图片不需要运行业务服务。项目历史主机证据使用 Qt5.12.8；不同发行版的 Qt 版本要记录，不能沿用历史测试数字。

## 2. 获取源码并记录版本

```bash
git clone https://github.com/dshanpi/DshanControl.git
cd DshanControl
git rev-parse HEAD
git status --short
```

记下完整提交。若要严格复现本教程的应用基线，可在新克隆中执行：

```bash
git switch --detach 6c7a5e382fd44c159eeec71b7a19af67498f589e
```

旧应用基线不包含后来新增的教程文件，这是正常的。日常阅读使用在线教程，开发时选择适合自己的分支。

## 3. 编译桌面

在仓库根目录执行：

```bash
mkdir -p dist/hmi
cd dist/hmi
/usr/bin/qmake ../../components/hmi/src/omnigate-hmi.pro
make -j2
```

成功后应生成 `dist/hmi/omnigate-hmi`。如果出现找不到 QtCharts 或头文件，先检查开发包和 `qmake` 所属版本，不要跳过编译错误。

## 4. 无屏幕也能生成界面图

保持在 `dist/hmi`：

```bash
QT_QPA_PLATFORM=offscreen ./omnigate-hmi --desktop-render ../desktop-render
find ../desktop-render -maxdepth 1 -type f -name '*.png'
```

成功判据是进程正常结束并生成 PNG；这些文件是固定示例渲染。`offscreen` 不会打开可操作的窗口，也不验证触摸和物理显示。

## 5. 打开窗口，体验桌面

在有图形桌面的 Ubuntu 会话中执行：

```bash
mkdir -p "$HOME/.local/share/dshancontrol/desktop"
chmod 700 "$HOME/.local/share/dshancontrol/desktop"
DSHANCONTROL_DESKTOP_STATE_DIR="$HOME/.local/share/dshancontrol/desktop" \
  ./omnigate-hmi --windowed
```

这里为主机指定个人布局目录，避免把目标板的 `/mnt/UDISK` 路径当作电脑的用户目录。以后用同一目录启动才能读取已保存布局。

SSH 纯终端没有图形会话时，先完成第 4 步；不要把 `could not connect to display` 误诊成应用源码坏了。

打开后依次检查：能看到工作台；能打开应用抽屉；能进入系统设置；能返回工作台。云、家庭、协议或媒体未配置时显示不可用，属于预期。窗口体验和隔离渲染是两个入口。

## 6. 验证开发环境

回到仓库根目录：

```bash
cd ../..
make test-bootstrap
make test HMI_QMAKE=/usr/bin/qmake
make test-hmi HMI_QMAKE=/usr/bin/qmake
```

记录失败与跳过，不能只看最后一行“完成”。测试会编译原生运行端、桌面后台、媒体工作进程与 Qt 用例，耗时比只编译 HMI 长。详细解释见[开发与测试](development.md)。

完成后继续[操作桌面](desktop.md)。常见编译和连接问题见[故障排查](troubleshooting.md)。命令来源：[Makefile](../../Makefile)、[主程序](../../components/hmi/src/main.cpp)。
