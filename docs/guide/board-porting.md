# 构建 S4 用户态并接入新板卡

**适用对象：** 已有合法、匹配 SDK 的系统集成人员。**执行位置：** Linux 构建主机。本文不需要使用历史开发虚拟机，也不对任何实板执行烧录。

## S4 构建输入

目标为 `t113_s4/100ask`、SD NAND，硬件来源为 [100ASK_T113s4-SdNand_TinaSDK5](https://github.com/DongshanPI/100ASK_T113s4-SdNand_TinaSDK5)。公开板级说明记录 Linux 5.4、Buildroot 201902、Linaro 5.3.1、ARMv7-A softfp 和目标 Qt5.11.3。

工具链、sysroot、Qt 和 Cedar 必须来自同一明确系统配置。先记录源码提交和本地修改，不用“最新 SDK”代替可复现身份。

## 隔离构建用户态

在 DshanControl 仓库根目录执行，替换三个真实路径；输出目录使用新的独立目录：

```bash
python3 scripts/prepare_s4.py \
  --sdk /path/to/T113-tina5v1.2-sdk \
  --hardware /path/to/100ASK_T113s4-SdNand_TinaSDK5 \
  --output /path/to/new-output
/path/to/new-output/build.sh configure
/path/to/new-output/build.sh --jobs 4 application
```

准备脚本快照应用来源、硬件提交和配置，选择 SDK 已有编译器。源码有修改时记录为开发草稿；它不会替你建立生产资格。

完整用户态 rootfs 构建另需足够空间，工具说明要求至少 16 GiB 余量：

```bash
/path/to/new-output/build.sh --jobs 4 rootfs
```

这个产物仍不等于完整可烧录固件。BSP、bootloader、分区、rootfs 最终化和厂商打包要在匹配系统流程中完成。不要沿用 SDK 上一次选择的 SPI NAND 板配置。

## 系统集成的关键点

- S4 应用/数据卷需要匹配分区映射的可写 ext4 UDISK；启动失败不能自动格式化用户卷。
- BusyBox/init、服务角色、显示与更新 socket 必须匹配。
- `system-contract.json` 中的 pending 值是待完成项，不得手工改成通过来启用更新。
- 完整镜像必须记录 SDK 各组件、补丁、分区/容量、boot 信息和镜像哈希。
- 应用 ELF、用户态 rootfs 与完整 IMAGEWTY 镜像分别验证，不能相互替代。

## 接入另一块板的顺序

1. 建立独立板级目录和真实能力清单。
2. 先验证基础启动、存储和恢复，再验证显示、触摸、网络与时间。
3. 构建匹配 ABI 的 HMI、后台、媒体和原生运行端。
4. 分别接入实际存在的 CAN、RS485、蜂窝或其他接口，未提供的能力保持不可用。
5. 建立系统契约、公钥、维护许可、健康检查和数据边界。
6. 在真实板卡验证安装、重启、故障恢复及回退，再声明相应支持范围。

源码可移植不意味着各板已经支持。对共享引脚、输入范围、音频采样率和厂商私有 ABI 应采用本板证据。

更完整的命令和限制保留在 [S4 README](../../boards/t113s4-sdnand/README.md)、[硬件输入](../../boards/t113s4-sdnand/hardware-inputs.md)、[发布规则](../RELEASING.md)。
