# Board porting guide

新增板卡时按以下顺序集成：

1. 在 `platform/boards/` 增加板卡描述，在 `platform/components.json` 登记对应的
   `board-adapter-example`，接入材料放在 `boards/` 或 `deploy/`。未经实板验证仅声明
   `candidate_profiles`，`supported_profiles` 保持空；目录/profile 成员关系不代表实板支持。
2. 在目标根文件系统安装 `/etc/omnigate/platform.json`，为每个物理接口定义稳定的逻辑
   名称、kind、role 和 device。
3. 适配显示、触摸、网络、存储和协议驱动；不要修改通用 HMI/Web 去写死设备名。
4. 将平台组件接入目标构建系统，使用板级系统契约指定的持久数据路径。
   T153 保留 `/var/lib/omnigate`；S4 使用启动映射的可写 ext4 UDISK 下
   `/mnt/UDISK/dshancontrol/data`，与应用版本树分离，启动或挂载失败不得格式化。
5. 运行主机测试、Manifest 校验、完整镜像构建和实板短测。
6. 分别记录协议组件可用、接口枚举、外部从站互操作和长期老化，不能互相替代。

最低发布门槛包括：冷启动、API 权限、数据库完整性、服务自愈、真实 framebuffer、触摸
输入事件、无 panic/Oops/OOM、分区状态和镜像校验。Enhanced 还必须验证 OCI CPU/内存/
PID 限制、无硬件设备映射、流程持久化、掉电恢复和负载。

生产版本另需完成安全启动、签名 OTA、自动回滚、唯一设备身份、强制改密、防火墙、
禁用调试入口和产品威胁模型；开发板短测不能作为 IEC 62443 认证。
