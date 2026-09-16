# DshanControl open-source release model

## 分层

1. 平台层：Manifest、本地数据库、`/api/v1`、Web、HMI、supervisor 和 profiles。
2. 协议适配层：CAN/CANopen、Modbus、EtherCAT、MQTT 和蜂窝接口。
3. 板级支持层：内核、DTS、启动配置和特定板卡固件，保存在各板卡集成项目中。
4. 厂商 SDK/工具链：遵循原厂条款，不因接入 DshanControl 自动变成 GPL 软件。

本仓库的 `make package` 只生成 `dshancontrol-<version>.tar.gz` 开源平台源码包及其
SHA-256。完整固件发布还必须附带精确镜像校验、分区清单、构建配置、SBOM、第三方
声明和实板验证报告。

```sh
make test-bootstrap
make test
make package
```

## 许可证边界

本仓库原创代码按 GPL-3.0 发布，第三方组件保留各自许可证。无线固件、Allwinner BSP、
厂商烧录工具和完整 Tina SDK 不进入本仓库。包含上述内容的板级集成包不能宣传为
“全部开源”。

## 验收措辞

只能使用 `PASS`、`FAIL` 或 `PASS WITH PERIPHERAL LIMITATIONS`。主机 Node-RED 测试
不是 Enhanced 实板测试；接口枚举不是协议互操作；开发安全测试不是 IEC 62443 认证。
