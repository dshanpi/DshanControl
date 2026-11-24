# 对接 DshanControl 桌面后端

适用于当前 Qt5 桌面、Flask 本地 API 和原生桌面服务。i.MX6ULL Pro 使用 `127.0.0.1:8080`；以下命令在运行 DshanControl 的主机内执行。接口可用不等于对应板级硬件已验证。

## 选择调用入口

| 调用方与用途 | 入口 | 身份与返回约定 |
| --- | --- | --- |
| Qt 状态、告警、显示设置与动作 | HTTP `/api/hmi/v1/` | 仅接受 loopback 请求；成功 `ok:true`，错误 `ok:false,error:string` |
| 启动检查、功能心跳 | HTTP `/api/health` | 返回 `ok`、`service`、`application_commit` |
| Qt 云端、点位、家庭与语音 | Unix socket `/run/dshancontrol/desktop/desktop.sock` | 校验 peer UID，使用 `protocol:1` RPC |
| 浏览器管理界面 | HTTP `/api/v1/` 及其他管理路由 | 独立的登录会话、角色与 CSRF 机制；不套用 HMI 的免登录规则 |

桌面中的实时点位来自桌面服务的 `reporting.points`。Web 管理接口中的点位配置记录不是实时采样数据；配置成功、设备在线和采样有效是三个不同状态。

## 配置同一台机器上的 HTTP 端口

| 变量 | 所属进程 | i.MX6ULL Pro 配置 |
| --- | --- | --- |
| `OMNIGATE_LISTEN_ADDRESS` | Flask | `127.0.0.1` |
| `OMNIGATE_LISTEN_PORT` | Flask | `8080` |
| `DSHANCONTROL_HMI_API_PORT` | Qt | `8080` |
| `HEALTH_URL` | Qt/启动管理器 | 通常不设置；显式设置时覆盖健康检查地址 |

Qt 未设置端口时保持旧板的 80 端口行为。设置端口后，业务路径与默认健康检查路径共同使用该端口。端口必须为 1–65535；不能通过这个变量改变主机名或路径。显式 `HEALTH_URL` 仍由 `ApplicationHeartbeat` 校验，要求 HTTP、`127.0.0.1` 和 `/api/health`，不接受账号、查询参数或重定向。

```sh
curl -fsS http://127.0.0.1:8080/api/health
curl -fsS http://127.0.0.1:8080/api/hmi/v1/snapshot
```

健康检查中的 `service` 应为 `omnigate-web`。开发镜像未配置应用安装身份时，`application_commit` 可以为空；不能把空值当作 OTA 身份验证通过。

## 读取状态与历史

| 方法与路径 | 请求 | 成功响应中的业务字段 |
| --- | --- | --- |
| `GET /snapshot` | 无请求体 | `timestamp,system,platform,summary,buses,networks,cellular,sensors,alarms,services,settings` |
| `GET /history` | `metric=cpu\|memory\|temperature\|storage`；`hours=1..720`，默认 24 | `metric,points:[{bucket,value}]` |
| `GET /alarms` | `limit=1..500`，默认 100 | `alarms:[]` |
| `GET /audit` | 无请求体 | `audit:[]`，最近 200 条 |
| `GET /settings` | 无请求体 | `settings:{...}` |

本表路径均接在 `/api/hmi/v1` 后。时间戳 `timestamp`、`bucket`、告警 `ts` 使用 Unix 秒。CPU、内存、温度、存储等测量值不可用时为 `null`，不能在界面中替换为正常的零值。

`system.active_slot` 只有在启动参数中明确出现唯一的 `rootfsA` 或 `rootfsB` 标记时才返回对应值。单根文件系统、未识别或矛盾的启动参数返回 `null`；i.MX6ULL 的 `/dev/mmcblk1p2` 不会被推断为 A 槽。

CAN 状态项的 `name` 可以为后端返回的 `CAN0`。界面按大小写无关方式归类；实际控制所用 `interface` 仍取平台通道中的设备名，例如 `can0`，不能从展示名称直接生成控制目标。

`networks[].online` 只表示链路状态。`source=demo` 为演示数据，`source=unavailable` 为不可用；不要据此声明现场设备或云端已经连通。

## 保存设置与确认告警

`PUT /settings` 接受 JSON 对象，可以只提供需要修改的字段。未提供的字段保留原值，未知字段返回 400。

| 字段 | 类型与范围 |
| --- | --- |
| `demo_mode` | JSON 布尔值，不能用字符串或 0/1 代替 |
| `brightness` | 整数 10–255，保存期望值 |
| `refresh_ms` | 整数 500–10000 |
| `page_cycle_seconds` | 整数 0–300；0 关闭轮播 |

```sh
curl -fsS -X PUT http://127.0.0.1:8080/api/hmi/v1/settings \
  -H 'Content-Type: application/json' -d '{"refresh_ms":1500}'
```

成功响应为 `{"ok":true,"settings":{...}}`。重新读取 `/settings` 与 `/snapshot` 时，应看到相同的设置。保存亮度只写入配置；实际应用背光属于独立动作，必须检查动作结果及硬件读回值。

告警确认使用 `POST /alarms/{id}/ack`，请求体为空或 `{}`。ID 范围为 1–9007199254740991。成功返回 `ok:true,id`；重复确认已确认的告警还返回 `already_acknowledged:true`。不存在或当前不可见的告警为 404，不可确认的状态为 409。界面不得在收到响应前提前显示确认成功。

## 提交动作并判断结果

`POST /actions` 要求以下三个顶层字段，不接受额外字段：

```json
{
  "action": "brightness",
  "params": {"value": 120},
  "confirmed": true
}
```

`action` 必须为非空字符串，`params` 必须为对象，`confirmed` 必须是 JSON `true`。`null`、数组、字符串及缺失参数不会被转换为默认对象；不符合约定的请求在执行动作前返回 400。

| Qt 动作 | `params` 中使用的字段 | 主要限制 |
| --- | --- | --- |
| `brightness` | `value` | 10–255；依赖已配置的背光执行路径 |
| `service` | `service,operation` | 服务必须可用；操作为 start/stop/restart |
| `can_apply` | `interface,bitrate,fd,data_bitrate,restart_ms` | 板级 CAN 通道；CAN-FD 需能力声明 |
| `canopen_nmt` | `interface,node_id,state` | CANopen 能力；节点 0–127 |
| `canopen_sdo_write` | `interface,node_id,index,subindex,size,value` | CANopen 能力；节点 1–127，数据宽度 1–4 字节 |
| `rs485_config` | `port,baudrate,unit_id,poll_ms` | 已映射的 Modbus RTU 通道与从站模板 |
| `ethercat_sdo_write` | `interface,slave,index,subindex,value` | 唯一的 EtherCAT 通道；值为 1–32 字节十六进制文本 |
| `ethercat_cycle` | `interface,count,period_us` | 总测试时长不超过 10 秒 |
| `ec20` | `profile,device,interface,operation` | 已验证的蜂窝设备映射 |

成功响应为 `{"ok":true,"action":"...","result":{...}}`。收到超时或断连后，实际结果可能尚未确认，不能自动重复控制请求。i.MX6ULL 开发镜像没有因此获得未验证的现场总线、背光、媒体或 OTA 能力。

## 对接桌面服务 RPC

每个连接发送一条以换行结束的 JSON 请求，响应也为一行。请求最多 65536 字节；Qt 客户端最多接受 524288 字节响应。

```json
{"protocol":1,"method":"desktop.snapshot","params":{}}
```

成功封装为 `{"protocol":1,"ok":true,"result":{...}}`；错误封装为 `{"protocol":1,"ok":false,"error":{"code":"...","stage":"...","message":"..."}}`。这里的 `error` 是对象，与 HTTP HMI API 的错误字符串不同。

| 方法 | 参数 | 结果语义 |
| --- | --- | --- |
| `desktop.snapshot` | `{}` | `result.version=1`，含 `cloud,reporting,home,voice` |
| `cloud.refresh` | `{}` | 请求刷新设备归属 |
| `reporting.configure` | `enabled:boolean,period_seconds:30\|60\|300` | 保存本机上报设置 |
| `reporting.send` | `{}` | 请求发送有效采样 |
| `home.refresh` | `{}` | 请求同步家庭设备 |
| `home.functions` | `device_id` | 请求设备能力列表 |
| `home.command` | `device_id,code,value,command_id,revision,confirmed:true` | 检查授权、数据新鲜度与版本后提交 |
| `voice.command` | `command` | 请求语音状态操作 |
| `voice.clear` | `{}` | 清空当前语音历史 |

动作的 `result.accepted=true` 表示请求已受理，不代表云端确认或设备状态已经改变。Qt 随后读取快照，以 `pending`、`fresh`、采样时间和上报确认等状态更新界面。

桌面实时点位 ID 格式为 `设备/指标`；时间为带 `Z` 的 UTC ISO 8601 字符串。`quality=good` 还必须满足类型、单位、有限数值和 60 秒新鲜度要求；`demo`、`fixture` 不作为有效实测点位。不要与 Web 配置记录的 ID 或 HTTP 历史的 Unix 秒时间戳混用。

## 验证接口一致性

在 Ubuntu 源码根目录执行 `make test` 和 `make test-hmi`。回归检查覆盖请求类型、参数缺失、错误封装、未知启动槽位、CAN 展示名称，以及使用非 80 端口的实际 HTTP 心跳。

交叉编译后执行：

```sh
python3 scripts/smoke_imx6ull_qemu.py \
  --target /absolute/path/to/buildroot/target \
  --output /absolute/path/to/new-evidence
```

ARM 检查使用临时数据库，在真实 ARM Flask 进程上读取健康、快照、历史、告警、审计，保存并恢复刷新设置，验证非法请求的错误响应，再启动 ARM Qt 和桌面 RPC。成功时 `result.json` 中 `passed`、`http_contract`、`local_api`、`desktop_rpc` 均为 `true`；逐项 HTTP 状态保存在 `http-contract.json`。该流程不发送已确认的硬件控制请求。
