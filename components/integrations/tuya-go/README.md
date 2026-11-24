# 部署 Go 涂鸦网关

本服务运行在 Linux 网关主机，将涂鸦 Cloud API 转为 DshanControl 桌面家庭设备接口。板端继续运行 C++ 桌面服务。项目 Access Secret 保留在网关主机，板上仅配置受限令牌。

## 编译并验证

在 Ubuntu 安装 Go、GCC 和 SQLite 开发文件。仓库 CI 使用 Go 1.26.1。

```sh
sudo apt-get install build-essential libsqlite3-dev
make build-go-gateway
make test-go-gateway
```

生成文件为 `dist/tuya-go/dshancontrol-tuya-gateway`。测试覆盖真实 Qt 桌面经 HTTPS 连接网关、模拟涂鸦上游、签名、令牌刷新、权限、数据类型、并发幂等和重启后的未知命令拒绝。模拟上游通过不表示真实涂鸦账号已授权。

## 准备私有配置

使用独立 Linux 服务账号。配置文件、项目密钥、桌面令牌、TLS 私钥必须为该账号所有且权限为 `0600`；数据库父目录必须为该账号所有且权限为 `0700`。不要将这些文件提交到仓库。

配置结构如下。执行前替换路径、项目 ID 和设备 ID；`region` 必须与涂鸦项目所在区域一致。

```json
{
  "region": "cn",
  "access_id": "REPLACE_PROJECT_ACCESS_ID",
  "access_secret_file": "/var/lib/dshan-tuya/access-secret",
  "desktop_token_file": "/var/lib/dshan-tuya/desktop-token",
  "command_database": "/var/lib/dshan-tuya/commands.db",
  "devices": {"REPLACE_DEVICE_ID": ["switch_led"]},
  "listen": "0.0.0.0:9443",
  "tls_certificate": "/etc/dshan-tuya/server.pem",
  "tls_key": "/var/lib/dshan-tuya/server.key"
}
```

`devices` 同时限制可见设备和可写功能码。空功能码列表允许读取设备，但禁止控制。支持 Boolean、Integer 和 Enum；整数范围、步长和枚举项来自上游功能定义。桌面令牌使用 32～1024 个字母、数字或 `._~-` 字符。TLS 证书必须覆盖桌面配置中的主机名或 IP。

## 保留原命令记录

迁移旧 Python 网关时，先停止旧进程，再备份原数据库及其 WAL 文件，随后将 `command_database` 指向原数据库。Go 网关沿用原 `commands(key,digest,state)` 表，不删除记录。不要通过清空数据库解决重复命令拒绝。

每条命令在访问上游前写入并持久化 `unknown` 记录。响应丢失、进程退出或重启后，相同 `Idempotency-Key` 返回 HTTP 409。上游接受后只返回 `accepted=true,state_confirmed=false`；桌面需等待设备状态回读后才确认完成。

## 启动并配置桌面

使用服务账号执行：

```sh
/opt/dshan-tuya/dshancontrol-tuya-gateway --config /var/lib/dshan-tuya/gateway.json
```

服务要求 HTTPS，不提供明文降级。桌面的私有配置中设置 `home.endpoint`、`home.token_file`；私有 CA 可通过 `https_ca_file` 指定。项目 Access Secret 不复制到板端。

接口保持为：

| 方法与路径 | 请求或结果 |
|---|---|
| `GET /v1/devices` | 已授权设备和真实上报状态 |
| `GET /v1/devices/{id}/functions` | 可控制权限、功能码和类型限制 |
| `POST /v1/devices/{id}/commands` | `{"code":"switch_led","value":true}`；必须提供 `Idempotency-Key` |

所有请求都需 `Authorization: Bearer <受限令牌>`。返回 403 时检查设备/功能授权；409 时检查设备离线、功能缺失或命令已提交；502 表示上游或存储失败，不能据此自动重发控制。
