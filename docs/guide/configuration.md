# 部署接入配置并备份用户数据

适用于系统集成与维护人员。先确定配置由哪个进程使用，再部署对应文件；备份时区分静态配置、布局、媒体和持续写入的数据库。

## 1. 确认实际路径

以下为公开源码默认值，系统启动脚本可能通过环境变量或参数重定向。应以当前设备进程配置及交付说明为准。

| 内容 | 默认路径 | 重定向入口 |
| --- | --- | --- |
| HMI 显示设置 | `/etc/omnigate-hmi/config.json` | `OMNIGATE_HMI_SETTINGS`，供 Web 后台使用 |
| HMI SQLite 数据 | `/var/lib/omnigate-hmi/hmi.db` | `OMNIGATE_HMI_DB`，供 Web 后台使用 |
| Web 配置 | `/etc/omnigate-web/config.json` | `OMNIGATE_CONFIG` |
| Modbus 采集配置 | `/etc/thingsboard-gateway/config/modbus.json` | `OMNIGATE_MODBUS_CONFIG` |
| 桌面接入配置 | `/etc/dshancontrol/desktop.json` | 服务参数 `--config` |
| 桌面 socket | `/run/dshancontrol/desktop/desktop.sock` | 服务 `--socket`；HMI `DSHANCONTROL_DESKTOP_SOCKET` |
| 布局 | `/mnt/UDISK/dshancontrol/desktop/layout-v1.json` | HMI `DSHANCONTROL_DESKTOP_STATE_DIR` 修改父目录 |
| 媒体扫描目录 | `/mnt/UDISK/media` | HMI `DSHANCONTROL_MEDIA_DIRECTORY` |

历史 `omnigate` 名称仍属于实际路径和接口，不能仅因品牌名称不同批量替换。

## 2. 准备私有接入配置

先用[主机接入服务教程](desktop-service.md)验证最小配置。生产环境按系统服务身份创建文件：配置和凭据应为该服务用户拥有的普通文件，权限仅允许所有者访问；持久目录同样仅允许所有者访问，不使用软链接。

部署顺序为：确定运行用户 → 准备受限目录 → 写入配置与凭据 → 核对所有者和权限 → 按维护流程启动服务 → 读取状态。不要把空实验配置覆盖到已有设备。

## 3. 按业务补充参数

### 配置官网绑定与 MQTT

以下是**结构模板，不可直接连接真实服务**。将设备 ID、HTTPS 地址、broker、端口和凭据路径替换为登记系统提供的实际值，再合并到本机配置中：

```json
{
  "state_directory": "/path/to/private-state",
  "cloud": {
    "device_id": "registered-device-id",
    "pairing_url": "https://service.example.invalid/pairing",
    "secret_file": "/path/to/private/cloud-secret",
    "mqtt": {
      "host": "mqtt.example.invalid",
      "port": 8883,
      "tls": true,
      "ca_file": "/path/to/trusted-ca.pem"
    }
  }
}
```

MQTT 要求 TLS。`ca_file` 是可选的自定义 CA 文件；省略时使用运行环境的信任配置，不能通过关闭证书验证解决连接失败。当前云 MQTT 连接使用登记设备身份及同一云凭据。

`state_directory` 用于保存 `reporting.json`，是修改自动上报设置的前提。目录所有者和权限不合格时，应先修复配置，而不是放宽客户端检查。

### 配置家庭与语音

| 配置节点 | 必需信息 | 验证方式 |
| --- | --- | --- |
| `home` | HTTPS `endpoint`、受限 `token_file` | 同步授权范围内的房间、设备和属性 |
| `voice` | 本机 `socket`、实际运行时 `runtime_uid` | 核对对端身份、激活与录放音就绪状态 |
| `sensor_mqtt` | TLS broker、`client_id`、`username`、`password_file`、显式 `sources` | 检查主题、点位、时间及质量 |

传感器主题按 `devices/<device_id>/telemetry` 显式映射，设备和主题不能重复；每个来源还需定义实际点位。数据格式及完整合同见[接入设计](../design/cloud-home-integration-v1/README.md)和[provider 实现](../../components/desktop/src/providers.cpp)。不要将普通 MQTT 消息自动视为合法点位。

凭据应通过受控方式写入本机文件，不放入公开教程、Git、截图或问题报告。业务不使用时省略其配置节点，保持明确的未配置状态。

## 4. 备份已保存的工作台

先完成编辑并退出编辑态，确认实际布局路径。以下在**布局文件所在 Linux 系统、具备读取权限的维护终端**执行；将 `LAYOUT_FILE` 改为实际文件：

```bash
umask 077
LAYOUT_FILE=/mnt/UDISK/dshancontrol/desktop/layout-v1.json
BACKUP_DIR=$(mktemp -d "$HOME/dshancontrol-backup.XXXXXX")
test -f "$LAYOUT_FILE" && test ! -L "$LAYOUT_FILE" && \
  cp "$LAYOUT_FILE" "$BACKUP_DIR/layout-v1.json"
sha256sum "$BACKUP_DIR/layout-v1.json"
printf 'Backup directory: %s\n' "$BACKUP_DIR"
```

检查复制和哈希命令都成功，再记录应用版本、布局 schema 与备份时间。备份放在同一设备时还需按维护策略转存；`.before-reset` 只是一份恢复默认前的副本，不能代替独立备份。

## 5. 创建 SQLite 一致快照

运行中的数据库可能使用 WAL 或日志，仅复制 `.db` 会遗漏事务。仓库的 `scripts/snapshot_sqlite.py` 是开发实验函数模块，没有命令行入口或 `--help`。

在**数据库所在的受控 Linux 维护环境、匹配源码仓库根目录**执行。准备 Python3，将两个参数替换为实际源库与新备份路径；目标文件必须不存在，父目录已创建且仅自己可访问：

```bash
umask 077
python3 - /path/to/hmi.db /path/to/private-backup/hmi-snapshot.db <<'PY'
import sys
from scripts.snapshot_sqlite import snapshot
snapshot(sys.argv[1], sys.argv[2])
print('SQLite snapshot verified')
PY
sha256sum /path/to/private-backup/hmi-snapshot.db
```

成功判据：脚本退出码为 0、输出 `SQLite snapshot verified`，并取得快照 SHA256。模块以只读方式打开源库，使用 SQLite backup API，检查 `quick_check` 和辅助文件状态，默认期限为 30 秒。异常遗留文件不得当作成功快照。

没有匹配工具的目标设备应采用交付方支持的一致性备份流程，不从其他机器直接复制运行中的主库文件代替快照。

## 6. 备份配置、媒体并准备恢复

配置和凭据备份保持原有访问限制；媒体记录实际文件、路径和哈希。对每份备份记录所属系统/应用版本，避免将旧配置直接覆盖到不兼容版本。

恢复前依次完成：

1. 核验备份哈希、版本、schema 和数据格式。
2. 按设备维护流程停止相关写入进程。
3. 为当前数据再保留一份副本。
4. 按系统支持的恢复流程放回文件，恢复正确所有者与权限。
5. 启动服务并检查布局、数据读取和业务状态。

应用回退不等于数据恢复。v1 应用更新要求相同数据格式，系统镜像可能清空用户卷，更新前须另行确认。

技术依据：[私密配置校验](../../components/desktop/src/model.cpp)、[数据库快照](../../scripts/snapshot_sqlite.py)、[布局存储](../../components/hmi/src/desktop/desktoplayout.cpp)。
