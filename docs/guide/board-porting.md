# 构建 S4 用户态并规划板级移植

在 Linux 构建主机上使用匹配 SDK 构建 S4 用户态。前提是具备合法 SDK、硬件源码和独立输出空间；本流程生成用户态构建结果，完整固件还需要 BSP、分区和厂商打包流程。

## 1. 核对构建输入

| 输入 | 当前 S4 基线要求 |
| --- | --- |
| 板型与存储 | `t113_s4/100ask`，SD NAND |
| 硬件源码 | [100ASK_T113s4-SdNand_TinaSDK5](https://github.com/DongshanPI/100ASK_T113s4-SdNand_TinaSDK5) |
| 系统构成 | Linux 5.4、Buildroot 201902 |
| 工具链 | SDK 预置 Linaro 5.3.1，ARMv7-A softfp |
| Qt | 目标系统 Qt 5.11.3 及匹配库 |

记录应用提交、SDK 各组件提交、本地补丁与硬件源码版本。工具链、sysroot、Qt、Cedar 和目标库应来自同一明确系统配置。

## 2. 准备独立输出目录

在 **DshanControl 仓库根目录**执行，先替换三个路径：

- `--sdk`：已经准备好的 Tina SDK 根目录。
- `--hardware`：对应硬件源码根目录。
- `--output`：本次新建的独立输出目录，不复用其他板型输出。

```bash
python3 scripts/prepare_s4.py \
  --sdk /path/to/T113-tina5v1.2-sdk \
  --hardware /path/to/100ASK_T113s4-SdNand_TinaSDK5 \
  --output /path/to/new-output
```

检查准备过程正常结束，输出中生成 `build.sh` 及构建输入记录。脚本快照应用来源、硬件提交和配置；源码不干净时记录为开发草稿，不应改写为正式来源。

## 3. 配置并构建应用

在**构建主机**执行：

```bash
/path/to/new-output/build.sh configure
/path/to/new-output/build.sh --jobs 4 application
```

确认两步退出码均为 0，并保存工具生成的命令和构建日志。失败时先查看第一处错误，区分依赖、工具链、源码和链接问题；不要将主机库复制到目标库目录解决 ABI 错误。

准备工具只构建文档规定的用户态目标。输出中的板文件快照不等于已经编译 BSP。

## 4. 按需构建 rootfs

构建卷至少预留 16 GiB 可用空间，这是保守构建余量，不是设备容量或实测最低值。确认后执行：

```bash
/path/to/new-output/build.sh --jobs 4 rootfs
```

检查日志与产物是否完整，核对目标应用、运行库、启动脚本和所需资源。完整系统镜像还需记录 BSP/bootloader、分区、rootfs 最终化及厂商打包输入，不能直接把用户态 rootfs 作为可烧录固件。

## 5. 检查系统集成条件

1. 数据卷按分区映射提供可写 ext4 UDISK；启动失败时不能自动格式化已有用户卷。
2. 系统 init、进程角色、显示/更新 socket 和应用启动路径相互匹配。
3. 使用真实硬件修订、公钥及构建产物推导系统身份；`pending` 值不能手工改成通过。
4. 健康检查同时验证进程身份、GUI 事件循环、后台和数据库，而非仅检查 PID。
5. 媒体和显示适配核对厂商 ABI，保留原始库、源码、补丁和许可记录。

具体系统契约最终化和镜像预检命令按[S4 板级说明](../../boards/t113s4-sdnand/README.md)执行，参数必须来自本次真实输入。

## 6. 接入新的板卡

按“启动与恢复 → 存储 → 显示/触摸 → 网络/时间 → 应用 → 外部业务 → 更新/回退”的顺序移植，每步保留结果。

建立独立能力清单，只声明实际存在的 CAN、串口、蜂窝或其他接口。板卡之间的共享引脚、触摸范围、音频和存储布局分别核对，不照搬 T153 或其他项目配置。

最终按[验收模板](acceptance.md)记录真实设备结果。构建成功证明产物生成，设备支持范围仍由该板的运行和恢复验证决定。

技术依据：[准备工具](../../scripts/prepare_s4.py)、[S4 README](../../boards/t113s4-sdnand/README.md)、[硬件输入](../../boards/t113s4-sdnand/hardware-inputs.md)。
