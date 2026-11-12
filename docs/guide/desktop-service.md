# 在主机启动并检查接入服务

在 Ubuntu 普通用户环境中启动独立的桌面接入服务，发送一条只读请求，再让 HMI 连接同一服务。完成后可以区分“服务未连接”和“业务尚未配置”。

前提：已获取源码并安装[快速开始](quickstart.md)中的依赖。以下操作只使用新建实验目录，不需要云密钥、家庭账号或真实设备。

## 1. 编译服务

在 **Ubuntu 主机、仓库根目录**执行：

```bash
make build-desktop HMI_QMAKE=/usr/bin/qmake
test -x dist/desktop-native/dshancontrol-desktop-service
```

`test` 命令成功时退出码为 0。该构建目标同时生成模型测试程序和通用媒体工作进程；通用主机构建不包含目标板 Cedar 解码能力。

## 2. 建立独立实验目录

在同一终端执行：

```bash
umask 077
LAB_DIR=$(mktemp -d /tmp/dc-guide.XXXXXX)
mkdir "$LAB_DIR/state"
python3 - "$LAB_DIR" <<'PY'
import json
import pathlib
import sys
root = pathlib.Path(sys.argv[1])
(root / 'desktop.json').write_text(json.dumps({
    'state_directory': str(root / 'state')
}) + '\n')
PY
chmod 600 "$LAB_DIR/desktop.json"
printf 'LAB_DIR=%s\n' "$LAB_DIR"
```

保存输出的完整目录。该配置只启用本地状态目录，不启用外部业务。目录权限为 `700`、配置文件为 `600`，均属于当前用户。Unix socket 路径应保持简短。

## 3. 启动服务并保留终端

保持在**仓库根目录、终端 A**：

```bash
./dist/desktop-native/dshancontrol-desktop-service \
  --socket "$LAB_DIR/desktop.sock" \
  --config "$LAB_DIR/desktop.json"
```

服务在前台运行，正常情况下不会立即返回命令提示符。若退出并输出 JSON 错误，查看 `stage`、`code` 和 `message`；配置权限、目录归属或 socket 冲突需要先解决。

## 4. 发送只读快照请求

打开**同一 Ubuntu 用户的终端 B**。将下方路径替换为第 2 步输出的真实目录，保留 `/desktop.sock`：

```bash
python3 - /tmp/dc-guide.XXXXXX/desktop.sock <<'PY'
import json
import socket
import sys
request = {'protocol': 1, 'method': 'desktop.snapshot', 'params': {}}
with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
    client.settimeout(5)
    client.connect(sys.argv[1])
    client.sendall((json.dumps(request) + '\n').encode())
    data = b''
    while b'\n' not in data:
        chunk = client.recv(65536)
        if not chunk:
            raise RuntimeError('连接结束但没有完整响应')
        data += chunk
        if len(data) > 524288:
            raise RuntimeError('响应超过检查上限')
response = json.loads(data)
assert response['protocol'] == 1 and response['ok'] is True, response
print(json.dumps(response['result'], ensure_ascii=False, indent=2))
PY
```

检查命令正常结束，响应的 `ok` 为 `true`。空业务配置下，云/家庭的 `configured` 为 `false`，语音未就绪；这些结果表示服务能够返回真实状态，不能解释为外部业务连接成功。

## 5. 让 HMI 连接实验服务

在**有图形会话的终端 B、仓库根目录**执行。把 `LAB_DIR` 替换为第 2 步生成的目录：

```bash
LAB_DIR=/tmp/dc-guide.XXXXXX
mkdir -p "$LAB_DIR/layout"
DSHANCONTROL_DESKTOP_SOCKET="$LAB_DIR/desktop.sock" \
DSHANCONTROL_DESKTOP_UID="$(id -u)" \
DSHANCONTROL_DESKTOP_STATE_DIR="$LAB_DIR/layout" \
  ./dist/hmi/omnigate-hmi --windowed
```

进入“官网绑定”或“智能家庭”。接入服务连接成功后，页面应显示相应业务未配置或未授权，而不是使用演示账号。`DSHANCONTROL_DESKTOP_UID` 必须与服务运行用户一致，客户端会检查服务端身份。

本步骤只连接桌面接入服务。运行总览、设置、告警等页面使用另一条 HTTP API 链路，不会因此自动获得数据，见[系统架构](architecture.md)。

## 6. 结束实验并保留记录

先正常关闭 HMI，再在终端 A 按 `Ctrl+C` 停止自己启动的服务。保留实验目录和响应用于复核。再次实验使用新的目录，避免把旧配置或旧 socket 当成本次结果。

| 异常 | 定位方法 |
| --- | --- |
| `No such file or directory` | 核对终端 B 的目录是否与终端 A 完全相同，确认服务尚在运行 |
| 配置/目录权限错误 | 用 `stat` 检查所有者及权限；不要通过放宽到 `777` 解决 |
| `服务身份不匹配` | 核对服务用户 UID 与 `DSHANCONTROL_DESKTOP_UID` |
| socket 正在使用 | 确认是否已有本次服务；不要删除正在使用的 socket |
| 业务仍未配置 | 本实验预期如此；按[配置参考](configuration.md)准备真实接入资料 |

技术依据：[服务参数](../../components/desktop/src/main.cpp)、[请求与身份校验](../../components/desktop/src/server.cpp)、[HMI 客户端](../../components/hmi/src/desktopclient.cpp)。
