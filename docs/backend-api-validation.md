# 后端接口对齐验证记录

验证日期：2026-11-24。执行主机：`192.168.1.228`，Ubuntu 24.04。范围为现有 Qt 桌面、Flask HMI API、原生桌面服务和 i.MX6ULL Pro 开发镜像；没有连接实板或发送硬件控制操作。

## 查看修正内容

| 项目 | 原行为 | 当前行为 |
| --- | --- | --- |
| 健康检查端口 | HMI 使用 8080 时，默认健康检查仍访问 80 | 业务接口与默认健康检查共用端口配置；保留显式 `HEALTH_URL` |
| CAN 状态名称 | 后端的 `CAN0` 无法被桌面的小写前缀匹配 | 大小写无关地归类展示名称，控制设备名保持平台映射 |
| 请求与错误封装 | 部分数组/空值被视为 `{}`，非空数组可能导致 500，错误缺少 `ok` | 校验 JSON 对象、必需字段与类型；HMI 错误统一为 `ok:false,error:string` |
| 启动槽位 | 未识别的启动布局默认显示 `rootfsA` | 仅接受明确且唯一的槽位标记，其余返回 `null` |

HMI 设置不再接受未知字段；动作请求必须包含 `action`、`params` 和 `confirmed`。当前 Qt 调用已使用这些字段。其他调用方应按 [后端接口说明](backend-api.md) 对接，不能依赖缺失字段或错误类型被隐式转换。

## 核对源码与构建输入

```sh
cd /home/ubuntu/dshancontrol-imx6ull-20261124
git -C source branch --show-current
cat api-alignment/provenance.json
cat api-alignment/image-build-result.json
```

分支为 `fix/imx6ull-api-alignment-20261124`。本次编译的应用提交为 `3a443b4c37aa92d32a5c08e3ac2aaf9245857468`；构建源码从干净提交导出并逐文件比对。验证报告和镜像检查脚本作为后续提交交付，交付源码提交号见 `delivery-manifest.json`。

这是在已验证 BSP 输出上的应用增量重编译，随后重新生成完整镜像。BSP、交叉工具链、Qt、Linux、U-Boot 和 DTB 沿用前次版本；没有声称从空目录重编整个 BSP。旧应用快照保留在 `api-alignment/previous-app-source-1591dfd/`，前一版交付包仍在工作根目录的 `delivery/`。

## 检查测试结果

| 检查 | 实际结果 | 证据（相对 `api-alignment/`） |
| --- | --- | --- |
| `make test` | 334 项：306 通过、28 跳过，退出码 0 | `host-test.log`、`host-test.exit` |
| `make test-hmi` | Qt 132 项通过；原生更新 bridge/流程检查通过，退出码 0 | `host-hmi.log`、`host-hmi.exit` |
| Web 接口回归，原依赖环境 | 38 项通过 | `web-tests.log` |
| Web 接口回归，Flask 3.1.3 | 38 项通过 | `flask31-tests.log` |
| ARM 应用和镜像构建 | 退出码 0 | `image-build-result.json` |
| ARM Flask/Qt/桌面 RPC 联调 | 通过，1024×600 截图已查看 | `arm-smoke/result.json`、`desktop-arm.png` |
| 最终镜像只读校验 | 通过 | `image-validation/result.json`、`e2fsck.log` |

28 项跳过仍涉及 TLS/MQTT fixture 工具及要求 root/特定权限的测试，没有计为通过。Qt 新增检查覆盖后端 CAN 展示名称；心跳测试通过真实 HTTP 连接验证非 80 端口、后端身份匹配与 GUI 事件循环。

## 检查 ARM 接口调用记录

```sh
cd /home/ubuntu/dshancontrol-imx6ull-20261124/api-alignment
cat arm-smoke/result.json
cat arm-smoke/http-contract.json
```

`passed`、`http_contract`、`local_api`、`desktop_rpc`、`layout_created` 均为 `true`。12 次 HTTP 调用实际完成以下检查：

1. 读取健康、设置与快照，校验成功封装。
2. 只修改刷新周期为 1500 ms，确认其他设置不变，且快照与保存结果一致。
3. 读取 CPU 历史、告警与审计，确认数组字段存在。
4. 提交错误类型的设置、拼错字段的设置和未确认的动作，均返回 400 与 `ok:false`。
5. 确认拒绝的请求没有改变设置，并恢复测试前的设置。

测试使用临时数据库和临时配置，不发送 `confirmed:true` 的硬件动作。RPC 快照来自真实 ARM 桌面服务。Qt 截图显示中文、布局和“本地数据已连接”正常。

## 取得新版镜像与源码

228 上的新交付目录：

```text
/home/ubuntu/dshancontrol-imx6ull-20261124/api-alignment/delivery/
```

目录包含接口说明、本报告、完整源码包、相对上一版的补丁、压缩开发镜像、验证证据、截图、校验文件及交付清单。源码包由干净 Git 提交导出。完整镜像仍为 536 MiB，根文件系统为 512 MiB ext4。

新版 `sdcard.img` 的 SHA-256：

```text
b6632067761a1f652f0f147f1147b6ba659b95cce932014440034427019275c3
```

在交付目录执行 `sha256sum -c SHA256SUMS` 检查下载文件。保留 `IMAGE-SHA256SUMS`，用于检查独立镜像文件。压缩镜像已经通过 `xz -t` 检查。

镜像校验确认：四个 ARM 原生程序与模拟运行时的程序哈希一致；镜像中的 `app.py`、`hmi_api.py` 与构建提交逐字节一致；启动分区、ext4、用户权限和设备树检查通过。

实板启动、显示/触摸和驱动仍需上板验收。视频解码、OTA 和未声明能力的外设没有因本次接口对齐而启用。
