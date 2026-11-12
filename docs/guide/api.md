# 调用本机查询接口并处理异步结果

供页面与接入开发者查阅。先完成[架构定位](architecture.md)和[桌面服务实验](desktop-service.md)，区分 HTTP API、desktop IPC 与系统更新/媒体协议。

## 1. 查询 HMI HTTP API

HMI 默认使用 `http://127.0.0.1/api/hmi/v1`。这些接口走本机边界，普通 Web 管理接口另有认证规则。

| 方法与路径 | 输入或范围 | 返回用途 |
| --- | --- | --- |
| `GET /api/health` | 无 | Web 健康状态 |
| `GET /api/hmi/v1/snapshot` | 无 | 系统、接口和业务快照 |
| `GET /api/hmi/v1/history` | `metric`、`hours` | 系统指标历史点 |
| `GET /api/hmi/v1/alarms` | `limit=1..500`，默认 100 | 最近告警集合 |
| `POST /api/hmi/v1/alarms/<id>/ack` | 空对象或空请求体 | 确认指定可见告警 |
| `GET /api/hmi/v1/settings` | 无 | 当前 HMI 设置 |
| `PUT /api/hmi/v1/settings` | 合法设置字段 | 写入并返回设置 |
| `POST /api/hmi/v1/actions` | 动作名、参数及确认状态 | 白名单业务动作 |
| `GET /api/hmi/v1/audit` | 无 | 最近 200 条审计记录 |

在**已部署设备的本机终端**执行只读示例：

```bash
curl --fail --silent --show-error http://127.0.0.1/api/health
curl --fail --silent --show-error \
  'http://127.0.0.1/api/hmi/v1/history?metric=temperature&hours=1'
curl --fail --silent --show-error \
  'http://127.0.0.1/api/hmi/v1/alarms?limit=20'
```

检查 HTTP 状态、JSON 结构和业务字段，不只判断命令是否输出文本。`history.metric` 仅允许 `cpu`、`memory`、`temperature`、`storage`，`hours` 为 `1..720`；它不是任意工业点位的通用历史库接口。

远程管理应使用适用的认证入口，不删除 loopback 限制来复用 HMI 接口。

## 2. 构造 desktop IPC 请求

默认 socket 为 `/run/dshancontrol/desktop/desktop.sock`。协议使用单行 UTF-8 JSON，一条连接处理一个有界请求；末尾必须有一个换行，服务端请求上限为 65536 字节。

只读请求内容：

```json
{"protocol":1,"method":"desktop.snapshot","params":{}}
```

成功响应的外层为 `protocol`、`ok: true`、`result`；失败响应为 `ok: false` 及 `error.code`、`error.stage`、`error.message`。不要将附加字段或多条换行 JSON 拼入同一次请求。

完整 Python 调用和实际结果检查见[接入服务实验](desktop-service.md)。HMI 用 `DSHANCONTROL_DESKTOP_SOCKET` 选择路径，并以 `DSHANCONTROL_DESKTOP_UID` 检查服务对端身份；服务端也检查调用方 UID。

## 3. 选择已实现的方法

| 方法 | `params` | 需要注意 |
| --- | --- | --- |
| `desktop.snapshot` | `{}` | 读取状态，Web 角色仅获得允许的只读范围 |
| `cloud.refresh` | `{}` | 查询登记归属/绑定码，异步返回状态 |
| `reporting.configure` | `enabled`、`period_seconds` | 要求持久目录，周期仅 30/60/300 秒 |
| `reporting.send` | `{}` | 要求 MQTT 在线、有有效点位且无在途上报 |
| `home.refresh` | `{}` | 刷新授权和设备状态 |
| `home.functions` | `device_id` | 读取目标设备可用属性 |
| `home.command` | 设备、属性、值、命令 ID、修订和确认字段 | 必须绑定最新能力与授权，详见 provider 合同 |
| `voice.command` | `command` | 运行时就绪后执行允许的语音命令 |
| `voice.clear` | `{}` | 清理本次服务会话记录 |

字段严格按方法校验。示例结构只能说明协议，外部业务仍需实际配置与权限。控制请求应由产品流程验证目标和维护条件，不将内部方法作为任意 shell 入口。

## 4. 处理请求受理与最终状态

请求返回 `accepted` 后，通过后续快照检查实际阶段。建议页面分别展示：未配置、未连接、等待、进行中、成功、失败和结果未知。

| 错误或状态 | 客户端处理 |
| --- | --- |
| `not-configured` | 引导准备对应登记/授权配置 |
| `not-connected` | 检查连接，读取新状态 |
| `no-data` | 检查点位有效性与时效 |
| `not-ready` | 检查语音激活和录放音状态 |
| `stale-or-denied` | 重新同步授权与能力后再操作 |
| `read-only` | 保留只读展示，不提供写入动作 |
| `busy` | 等待当前请求结束 |
| 超时 / 断线 / 结果未知 | 查询原对象/任务结果，避免自动重发控制 |

## 5. 接入新点位

为每个点位明确稳定身份、数值类型、单位、范围、时间、质量和过期策略。MQTT 输入使用显式来源和 `devices/<device_id>/telemetry` 主题映射；retained 消息不能替代实时样本。

先验证正常样本、重复身份、非法类型、过期、断线与恢复，再接入真实源。缺测保留为空，不使用零或随机数据补齐。

技术依据：[HTTP API](../../components/web/hmi_api.py)、[IPC 服务](../../components/desktop/src/server.cpp)、[方法与点位处理](../../components/desktop/src/providers.cpp)。
