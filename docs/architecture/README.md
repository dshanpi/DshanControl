# Qt5 应用框架架构

新版 README 使用 docs/images/qt5-desktop/architecture.svg。该图描述源码中的交互框架、本机服务、系统拥有的治理边界及外部接口，不是当前板端部署图。

应用入口由 DesktopShell → ApplicationRegistry → MainWindow 业务页面组织。注册器校验 ID 和上下文；UI 通过本机 API 或 IPC 使用后台。Native Runtime 负责服务角色、进程所有权、心跳与更新事务。真实现场维护许可由系统核验，不能用实验室脚本或 UI 状态代替。

Desktop Service 的目标独立角色为 UID402，跨启动迁移及真实业务仍待验收；不因源码存在就标记现场已部署。布局、配置和业务数据保留独立边界，不进入公开源码包。
