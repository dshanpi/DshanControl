# DT 逐页功能与流程复查

用户要求继续优化全部页面、业务逻辑与功能。本轮保留已认可方向，先审查24页、业务契约和代表Qt截图，再按明确问题修改。用户细节批准仍保持原记录；统一实施授权单独引用。

| 页面 | 核对功能 | 设计与实现决定 | 回归入口 |
|---|---|---|---|
| P01 运行总览 | 采集来源、在线数、60秒双曲线、活动告警、链路切换与监控跳转 | 保留有效值/离线/样例标识；不把界面在线数当硬件验收 | refinedMonitorAndOverviewNavigationPreserveReadings |
| P02 设备监控 | 读数表、温压趋势切换、本页暂停/继续、显示设置入口 | 暂停只冻结本页；返回后新段恢复；缺测不补零 | monitoringActionsStayVisibleAndExplainPausedDisplay |
| P03 绑定点位趋势 | 主/对照绑定、稳定ID、单位质量、共享历史、离线历史 | 使用共享历史，不新增采集；保留缺测断段和窄屏质量信息 | trendContextUsesSharedStablePointHistoryAndLegacyRouteStillWorks |
| P04 全部告警 | 状态/级别/来源/时间筛选、详情、确认、CSV导出 | 可见ID约束；导出冻结点击时快照；离线保留记录只读 | alarmEmptyFilterRecoveryAndRetainedRecordsAreExplicit |
| P05 未确认告警 | 未确认筛选、ID详情、单次确认、回复刷新 | 确认不消除故障；pending禁止重入，失败保留记录 | historySelectionTracksAlarmId |
| P06 已确认告警 | 已确认筛选、详情、导出、空态恢复 | 已确认不可再次确认；筛选隐藏选择后不操作旧行 | alarmEmptyFilterRecoveryAndRetainedRecordsAreExplicit |
| P07 CANopen · NMT | CAN接口/FD/仲裁与数据速率配置、NMT接口节点状态 | 广播节点0明确；确认绑定参数/连接；能力缺失禁止提交 | actionConfirmationRejectsReconnectAndKeepsFailure |
| P08 CANopen · SDO | CAN公共配置、SDO索引/子索引/32位值写入 | 保留4字节写入和无读回边界；确认与结果固定 | primaryFormActionsRemainVisibleAtDesktopResolution |
| P09 RS485 · Modbus | 首个RS485从站端口/波特率/站号/周期保存 | 保存配置不冒充运行生效；无寄存器读回能力如实说明 | subpageGuidesFollowActualSelection |
| P10 EtherCAT | EtherCAT从站/索引/子索引/1–32字节写入、1000次周期测试 | 两个操作独立确认；固定周期参数及能力门保留 | actionConfirmationRejectsReconnectAndKeepsFailure |
| P11 有线网络 | 有线接口列表、稳定接口选择、地址/来源详情、刷新 | 只读；地址不证明目标服务可达，选择消失不静默换对象 | refinedPagesPreserveStateAndExposeRecovery |
| P12 无线网络 | 无线接口列表、详情、刷新、在线空列表/离线区别 | 不新增未实现的扫描、密码或网络配置；空列表仍可刷新 | refinedPagesPreserveStateAndExposeRecovery |
| P13 蜂窝网络 | 双蜂窝映射、拨号/断开、地址与模块状态 | 能力与映射复核；断线重连拒绝旧确认，结果不冒充连通 | actionConfirmationRejectsReconnectAndKeepsFailure |
| P14 显示与交互 | 亮度10–255、刷新周期、演示开关、撤销、保存/背光、色彩校验 | 编辑与服务记录分开；保存与应用各自核回包，撤销不写硬件 | settingsDraftRestoreIsExplicitAndDoesNotApplyHardware |
| P15 服务管理 | ThingsBoard/蓝牙/NTP启停重启、整机维护重启 | 保持真实可用性、默认取消与连接变化拒绝；日志入口核对 | actionConfirmationRejectsReconnectAndKeepsFailure |
| P16 操作日志 | 日志刷新、结果筛选、选中摘要、纯文本完整详情 | 筛选隐藏时清空选择；不通过重新执行控制制造日志 | auditDetailsAreTouchAccessibleAndBoundToSelection |
| P17 官网绑定 | 设备身份、云连接、绑定码/有效期、查询/再次查归属、帮助 | 新增本地等待锁及独立反馈；缩小说明间距以保留48px按钮和底部结果 | cloudPendingRequestsLockControlsAndPreserveDraft |
| P18 数据上报 | 有效点位列表、自动上报开关/周期、手动上报、代理确认 | 提交到刷新完成期间锁定同类操作；旧快照不覆盖提交中设置；反馈独立 | acceptedSendWaitsForSnapshotBeforeUnlocking |
| P19 语音对话 | 按住说话、自由对话、结束、切页停止、记录与清空 | 真正离线快照不再清空记录；服务恢复明确提供空历史才清空；语音释放队列保持 | voiceDisconnectRetainsTranscriptUntilExplicitServiceHistory |
| P20 家庭设备 | 房间筛选、稳定设备选择、能力控件、同步、确认、回报等待 | 隐藏/移除设备清空控制；确认绑定状态世代与原revision，断线/能力/选择往返亦拒绝 | householdConfirmationBindsReviewedState |
| P21 视频播放器 | 文件选择、播放/暂停、上下个、进度、音量/静音、全屏 | 解码生命周期不改；共享扫描保持列表选择而不打开或播放 | mediaWorkflowUsesVisibleActionsAndSelectionDoesNotPlay |
| P22 本地视频列表 | 目录扫描、选中文件详情、明确选择、浏览其他位置 | 刷新按完整路径恢复选择；文件消失清空详情/禁用选择，不替换为其他文件 | mediaRefreshPreservesPathSelectionWithoutPlayback |
| P23 播放设置 | 画面比例、单文件循环、返回播放、解码状态 | 保留切页暂停/隐藏；共享列表调整不改变解码或循环控制 | mediaWorkflowUsesVisibleActionsAndSelectionDoesNotPlay |
| P24 应用更新 | 公共身份、检查签名兼容、下载、确认安装、恢复任务、结果 | 固定已下载身份；期间变化拒绝；中文48px确认；版本刷新保留结果与失败边界 | nativeUpdateTasksRequireDownloadedIdentityAndPreserveErrors |

公共界面覆盖工作台/卡片编辑/应用抽屉、步骤、控制确认、文件选择、告警导出、日志详情与更新结果。既有后台能力、签名/维护门、语音释放顺序和媒体生命周期均保留。

本轮新增功能是更准确的等待、选择与结果处理，未新增系统未提供的网络配置、现场总线读回或媒体能力。原失败和中间图全部保留。测试使用隔离主机服务与样例，不升级硬件业务验收。

最终补充：P24 的更新器进程启动/通信失败只表示本次请求结果未确认，不能声称没有安装。已知安装完成结果与后续身份读取失败分开显示，原任务阶段和错误码继续保留。

恢复步骤：更新服务请求失败后，点击“检查更新”先读取公共身份并恢复已有任务，禁止直接重复提交安装。

最终验证：完整 make test-hmi 四组 7/132/6/16 全通过，测试前后 tracked 源码 SHA 一致。make test 核心5通过、仓库386通过/13环境跳过；运行期间UI记录变化已分离归档，相关后端源码未变。逐页真实主机图与流程记录已绑定；构建/提交/源码包结果在SDK交付目录，DT尚未部署，原73项和物理业务资格不升级。
