# 在 Ubuntu 编译并运行桌面

在 Ubuntu 主机上编译 HMI（人机界面程序），生成界面图，并打开可交互的桌面窗口。此流程不依赖开发板或外部账号。

## 1. 准备主机环境

本流程已在 Ubuntu 24.04、Qt 5.15.13 上验证。Ubuntu 22.04 可采用相同包名安装依赖，但应记录实际 Qt 版本。准备普通用户终端、网络连接和可写工作目录；第 5 步还需要本机图形桌面会话。

在 **Ubuntu 主机终端**执行：

```bash
sudo apt-get update
sudo apt-get install -y build-essential git qtbase5-dev libqt5charts5-dev \
  libssl-dev zlib1g-dev libsqlite3-dev python3-venv python3-pil \
  mosquitto mosquitto-clients
/usr/bin/qmake -v
g++ --version
python3 --version
```

确认 `qmake -v` 显示 Qt 5.x。若提示 Qt6、缺少 qmake 或 QtCharts，先检查对应开发包；不要混用目标板 SDK 中的 ARM 库与主机编译器。

## 2. 获取源码并记录提交

以下示例将仓库放在 `$HOME/dshancontrol-work/DshanControl`。若已有同名仓库，先确认其用途和工作区状态，不要覆盖。

```bash
mkdir -p "$HOME/dshancontrol-work"
cd "$HOME/dshancontrol-work"
git clone https://github.com/dshanpi/DshanControl.git
cd DshanControl
git rev-parse HEAD
git status --short
```

保存完整提交号。新克隆的 `git status --short` 应没有输出。需要严格复现本手册应用基线时，可在这个**新克隆**中切换：

```bash
git switch --detach 6c7a5e382fd44c159eeec71b7a19af67498f589e
```

分离 HEAD 适合复现；正式修改代码前应创建自己的开发分支。该应用基线早于教程提交，在线手册可以继续使用。

## 3. 编译 HMI

在 **仓库根目录**执行：

```bash
mkdir -p dist/hmi
cd dist/hmi
/usr/bin/qmake ../../components/hmi/src/omnigate-hmi.pro
make -j2
test -x ./omnigate-hmi && printf 'HMI executable is ready\n'
```

成功后输出 `HMI executable is ready`，程序位于 `dist/hmi/omnigate-hmi`。编译失败时停在本步，检查第一处编译错误；末尾的 `make: Error` 通常只是汇总。

## 4. 生成界面图

保持在 **`dist/hmi`**，执行：

```bash
QT_QPA_PLATFORM=offscreen ./omnigate-hmi --desktop-render ../desktop-render
find ../desktop-render -maxdepth 1 -type f -name '*.png' -printf '%f\n'
```

当前基线应生成 10 张 PNG，包括工作台、抽屉、搜索、编辑和宽屏视图。打开图片检查中文、布局和组件是否完整，图片对应入口见[界面图索引](gallery.md)。

此命令正常结束并生成图片即可完成离屏检查。SSH 环境停在此处也能继续[接入服务实验](desktop-service.md)；`offscreen` 不提供鼠标可操作窗口。

## 5. 打开桌面窗口

在 **Ubuntu 图形会话中的终端**进入同一 `dist/hmi` 目录，执行：

```bash
mkdir -p "$HOME/.local/share/dshancontrol/desktop"
chmod 700 "$HOME/.local/share/dshancontrol/desktop"
DSHANCONTROL_DESKTOP_STATE_DIR="$HOME/.local/share/dshancontrol/desktop" \
  ./omnigate-hmi --windowed
```

该命令将主机布局保存到个人目录。后续启动应使用相同环境变量，才能读取同一份布局。

窗口打开后依次操作：

1. 打开“全部应用”，确认能看到应用分类和入口。
2. 进入“系统设置”，再返回工作台。
3. 进入“编辑工作台”，完成一次小修改并保存，具体操作见[保存工作台](widgets.md)。
4. 正常关闭窗口，使用上面命令再次启动，核对布局。

未启动 Web API 或桌面接入服务时，相关业务显示未连接、未配置或不可用。此时可检查导航和布局，不具备真实采集、云控制或视频解码条件。

## 6. 处理首次运行问题

| 现象 | 检查与处理 |
| --- | --- |
| `Unknown module(s) in QT: charts` | 安装 `libqt5charts5-dev`，确认使用 `/usr/bin/qmake` 后重新生成 Makefile |
| `could not connect to display` | 当前终端没有图形会话；使用第 4 步离屏检查，或在 Ubuntu 桌面内运行第 5 步 |
| 布局不能保存 | 检查个人布局目录的所有者和可写性；关闭重复编辑进程后重试 |
| 云、家庭或视频不可用 | 继续对应服务集成步骤；只编译 HMI 不会安装这些能力 |
| 程序直接退出 | 保留终端首个错误和退出码，按[故障排查](troubleshooting.md)检查字体、库和路径 |

## 完成检查

应保留源码提交、Qt 版本、编译结果和渲染输出。图形环境下还应确认窗口可打开、应用可进入/返回、布局可重新加载。

接下来进行[桌面操作](desktop.md)；需要调试云/家庭页面的本机连接时，继续[启动接入服务](desktop-service.md)。完整工程测试在[开发流程](development.md)中执行。

技术依据：[主程序入口](../../components/hmi/src/main.cpp)、[HMI 工程](../../components/hmi/src/omnigate-hmi.pro)。
