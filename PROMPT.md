完成 requirements.md 中本次 DshanControl 版本的需求，逐项实现、运行验证、修复失败并记录进度。
先读 AGENTS.md、requirements.md、docs/ITERATION-ACCEPTANCE.md、docs/UI-IMPLEMENTATION.md。
你接手的是包含大量已实现 UI 的未提交工作树，必须保留，不要重做整个项目或换技术栈。

每轮选择能推进的未通过项，检查现有实现与证据，修复实际问题，运行对应测试和目标交叉构建。
以 docs/ITERATION-ACCEPTANCE.md 的 S01—S12、H01—H05 逐项记录状态与日志路径，更新 scratchpad。
测试必须验证行为；不能削弱验收、替换成假通过、凭示例数据声称实板可用。
不要自动提交、推送、发布、联网发消息、烧录、格式化或删除既有成果。截图只存 dist/。
只启用一个串行执行者，不启动其他并行代理或另一个 Ralph 循环。

软件 S 项全部完成且有本轮有效证据后，通过 ralph emit acceptance.software.passed 记录事件。
只有实际取得 H 项全部验收证据后，才可发出 ralph emit acceptance.board.passed。
只有上述全部验收通过且无待办/外部阻塞时，才可发出 ralph emit LOOP_COMPLETE，并明确报告完成。
不要在普通日志、示例、cat 输出或未通过时声称完成。

若软件仍可推进就继续；若所有剩余任务均依赖实板、用户身份/签名资料或不足构建空间等外部条件，
在验收记录逐条列出阻塞条件和已完成工作，再发出 ralph emit DSHAN_EXTERNAL_BLOCKED，非成功结束。
不要在相同硬件缺失上空转 30 轮，也不要因此把未验收项改成通过。输出使用中文。

Ralph 2.10.1 本地执行兼容要求：所有 ralph emit 命令都将 stdout 追加到
`dist/ralph-run/emit-acks.log`，例如 `ralph emit task.done "本轮证据摘要" >> dist/ralph-run/emit-acks.log`。
这只收存 CLI 确认文字；事件仍写入原事件文件，stderr 与非零退出码必须保留并处理。
不要读取或回显这个确认文字文件；不要打印 dist/ralph-setup 内的执行器源码。
先写完验收表、scratchpad 和后续任务，再发出本轮最后事件，随后简短结束当前轮次。

用户2026-10-13补充：硬件MCP已提供且已授权试烧，主交互代理负责当前硬件，Ralph继续本地构建，勿并发操作硬件。
必须使用Tina厂商系统/厂商镜像，不改走mainline；AIC8800D80 WiFi6、RGB50 1024x600，文件只走网络、不得串口传文件。
实板串口只读证据在dist/hardware-mcp/board-identity.json和board-storage.json：allwinner,t113_s4、SD、2013265920字节、root p5、UDISK p9当前vfat。
后续读AGENTS.md新增硬件段及dist/hardware-mcp/user-handoff.json；不能再把“完全无硬件连接/未知容量”当作既定事实，仍需真实修订、完整镜像与上板验收。
