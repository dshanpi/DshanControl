# 管理配置、权限与数据备份

**适用对象：** 系统集成人员和维护人员。下面列出源码默认路径；板级运行脚本可能重定向到数据卷，应以正在运行的进程环境和交付说明为准。

## 配置分别属于哪一层

| 内容 | 默认路径或入口 | 说明 |
| --- | --- | --- |
| HMI 设置 | `/etc/omnigate-hmi/config.json` | 演示、亮度和刷新配置 |
| HMI 数据库 | `/var/lib/omnigate-hmi/hmi.db` | 趋势、告警、审计等 SQLite 数据 |
| Web 配置 | `/etc/omnigate-web/config.json` | 账号及部分协议配置，属于私密配置 |
| 桌面后台配置 | `/etc/dshancontrol/desktop.json` | 云、家庭、传感器、语音接入 |
| 桌面 IPC | `/run/dshancontrol/desktop/desktop.sock` | 运行期 socket，不是备份文件 |
| 工作台布局 | `/mnt/UDISK/dshancontrol/desktop/layout-v1.json` | 可用 `DSHANCONTROL_DESKTOP_STATE_DIR` 修改目录；文件名保持 `layout-v1.json` |
| 媒体 | `/mnt/UDISK/media` | 用户文件，不放入应用更新载荷 |

路径中的历史 `omnigate` 名称仍是实际接口的一部分，不要仅为品牌一致而批量改名。

## 桌面后台最小隔离配置

仅想检查服务能够启动而不连接外部业务时，可在 Ubuntu 普通用户目录准备空 JSON。它不会启用云、家庭或语音：

```bash
mkdir -p "$HOME/dshancontrol-lab"
chmod 700 "$HOME/dshancontrol-lab"
printf '{}\n' > "$HOME/dshancontrol-lab/desktop.json"
chmod 600 "$HOME/dshancontrol-lab/desktop.json"
```

不要把该空配置复制到生产设备覆盖已有配置。私密文件必须是运行服务用户拥有的普通文件，其他用户/组不能访问，禁止软链接。状态目录要求同一用户所有且仅所有者可访问。

## 业务接入字段

| 节点 | 需准备的真实信息 |
| --- | --- |
| `cloud` | 登记的 `device_id`、`pairing_url`、`secret_file`，及匹配 MQTT 配置 |
| `home` | `endpoint` 与受限授权的 `token_file` |
| `sensor_mqtt` | client/用户身份、`password_file` 和显式 `sources` 点位映射 |
| `state_directory` | 私有持久目录；用于保留上报配置 |

字段来自当前 provider，完整结构应对照[源码](../../components/desktop/src/providers.cpp)和[接入设计](../design/cloud-home-integration-v1/README.md)。页面不负责生成外部账号。凭据文件只在设备上受限保存，不提交到 Git，也不要在示例中写入真实 token。

## 工作台备份

先离开编辑态并完成保存，定位实际布局文件，复制到独立受限备份目录，记录应用提交和布局 schema。需要恢复时，在允许维护且相关程序已按流程停止后再替换，保留原文件以便回退。

布局自带 `.before-reset` 仅是一份恢复默认前的副本，不能代替版本化备份。不要在两个正在编辑的进程间直接覆盖文件。

## SQLite 一致备份

数据库运行时可能使用日志/WAL；只复制主 `.db` 文件可能遗漏事务。项目的 `scripts/snapshot_sqlite.py` 是开发实验用函数模块，**没有命令行入口，也没有 `--help` 子命令**，不会作为生产服务安装。

在具备匹配源码和 Python3 的受控维护环境，可从仓库根目录调用它；替换两个路径，目标文件必须尚不存在、父目录已存在且仅自己可访问：

```bash
umask 077
python3 - /path/to/hmi.db /path/to/private-backup/hmi-snapshot.db <<'PY'
import sys
from scripts.snapshot_sqlite import snapshot
snapshot(sys.argv[1], sys.argv[2])
print("SQLite snapshot verified")
PY
sha256sum /path/to/private-backup/hmi-snapshot.db
```

函数以只读模式打开源，使用 SQLite backup API，并检查新快照的 `quick_check` 和辅助文件状态；默认期限为 30 秒。异常中止的目标文件不能当成成功备份。若设备不具备这些工具，应使用该系统交付方支持的一致性备份流程，不以手动拷贝运行中的数据库替代。

## 应用更新与数据

应用版本目录、用户数据和系统底座必须分开。应用回退不应清空业务数据；v1 更新仅允许相同数据格式，不支持通过迁移脚本绕过。工厂系统镜像可能包含空 UDISK，烧录前单独核对清空范围。

来源：[HMI 默认路径](../../components/web/hmi_api.py)、[私密配置校验](../../components/desktop/src/model.cpp)、[布局原子写入](../../components/hmi/src/desktop/desktoplayout.cpp)。
