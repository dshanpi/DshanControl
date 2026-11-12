# 理解本机 HTTP API 与 IPC

**适用对象：** 开发业务页面或数据接入的工程师。本文解释调用边界，不把内部接口直接改为公网服务。

## HMI HTTP API

HMI 默认访问 `http://127.0.0.1/api/hmi/v1`；健康入口为 `/api/health`。`/api/hmi/v1/` 走本机访问边界，普通 Web 管理接口另有登录与权限约束。

| 路径 | 方法 | 用途 |
| --- | --- | --- |
| `/api/health` | GET | Web 健康状态 |
| `/api/hmi/v1/snapshot` | GET | 当前系统、接口及业务快照 |
| `/api/hmi/v1/history` | GET | 历史数据 |
| `/api/hmi/v1/alarms` | GET | 告警列表 |
| `/api/hmi/v1/alarms/<id>/ack` | POST | 确认指定告警 |
| `/api/hmi/v1/settings` | GET / PUT | 读取/保存 HMI 配置 |
| `/api/hmi/v1/actions` | POST | 白名单业务动作 |
| `/api/hmi/v1/audit` | GET | 审计记录 |

在已部署设备的本机终端可做只读检查（需系统有 curl）：

```bash
curl --fail --silent --show-error http://127.0.0.1/api/health
curl --fail --silent --show-error http://127.0.0.1/api/hmi/v1/snapshot
```

这些示例不发送控制命令。不要为了让远程调用成功而删除 loopback 检查；集成远程管理应设计独立认证与权限边界。

## 桌面服务 IPC

默认 socket 为 `/run/dshancontrol/desktop/desktop.sock`。HMI 可通过 `DSHANCONTROL_DESKTOP_SOCKET` 选择路径，并通过 `DSHANCONTROL_DESKTOP_UID` 核对服务端身份。路径可连接不等于身份合法。

常见方法包括 `desktop.snapshot`、`cloud.refresh`、`reporting.configure`、`reporting.send`、`home.refresh`、`voice.command`。参数严格按服务端方法定义，不能把它当任意 shell 执行入口。

主机隔离实验可以构建 `make build-desktop`，为服务指定自己的 socket 和私密配置；参数入口见 [main.cpp](../../components/desktop/src/main.cpp)。不要指向正在运行设备的生产 socket 做测试。

## 设计请求和结果

异步操作至少区分：未配置、等待、进行中、成功、失败、结果未知。界面应展示真实阶段和错误，禁止用定时器假定成功。系统更新需要绑定具体任务 ID；历史成功不是当前任务成功。

对于控制动作，提交前验证目标、参数、连接状态和维护条件；确认期间状态变化时拒绝旧确认。发生连接中断时先核对当前状态，避免重复执行。

## 点位接入的基本原则

MQTT sensor 配置通过显式 `sources` 映射接入，主题形如 `devices/<device_id>/telemetry`。主题、设备 ID 和点位定义要相互匹配。后端拒绝将 retained 旧消息当作实时读数，过期与缺测也不填零。

新 provider 需要同时设计：数值类型和范围、单位、时间/时效、质量标记、断线行为和来源身份。先用隔离测试验证错误路径，再连接真实数据源。

精确协议以 [server.cpp](../../components/desktop/src/server.cpp)、[providers.cpp](../../components/desktop/src/providers.cpp)、[hmi_api.py](../../components/web/hmi_api.py)为准。生产外部账号和密钥不属于示例配置。
