# Ralph 本地执行说明

本次用户明确要求整理需求并安装、启动 Ralph。安装版本为
`@ralph-orchestrator/ralph-cli@2.10.1`，后端使用本机已登录 Codex CLI 0.162.0。
参考：[Ralph CLI](https://mikeyobrien.github.io/ralph-orchestrator/guide/cli-reference/)。

## 输入与边界

- `requirements.md`：产品范围、S01—S12 软件验收、H01—H05 系统/实板验收。
- `PROMPT.md`：逐轮执行指令。
- `ralph.yml`：Codex 后端，最多 30 轮 / 4 小时，串行，禁止自动合并；关闭通知与外部消息。
- `docs/ITERATION-ACCEPTANCE.md`：人工可读验收进度；`.ralph/agent/scratchpad.md`：迭代工作记录。
- 本次不自动提交/推送/发布/烧录，保留已有工作树。进入循环前本地备份在 `dist/ralph-setup/`。
- 全部软件和硬件验收通过后才可成功结束；只有外部条件阻塞时发出专用取消事件，不能假报完成。

## 启动、观察、停止

在 `/home/ubuntu/DshanControl` 执行：

```sh
ralph preflight
ralph run --autonomous --verbose --no-auto-merge
```

2.10.1 的 `--no-tui` 与 `--autonomous` 互斥，因此这里仅使用后者。
不要在已有循环运行时重复启动。查询和停止命令：

```sh
ralph loops list --json
ralph loops stop
```

本次后台启动的终端日志、会话记录、进程与退出信息在 `dist/ralph-run/`。
可使用 `tail -f dist/ralph-run/output.log` 查看输出。
主循环的事件历史与详细诊断另保存在被 Git 忽略的 `.ralph/`。

停止后需要继续时，先阅读验收记录并解决外部条件，再执行：

```sh
ralph run --autonomous --verbose --no-auto-merge --continue
```

达到轮数/时间上限、后端失败、取消/外部阻塞，都不等于需求验收完成。
初始化和预检日志保存在 `dist/ralph-setup/`；该目录还包含安装来源和原始工作树备份。

## 进度可见性（本次排查后补充）

Ralph 2.10.1 的 Codex 适配使用文本模式，默认不显示 stderr；Codex 的工具执行过程
因此可能长时间不出现在总日志。后续启动/恢复增加 `--verbose` 显示这些输出。
独立观察器每 20 秒检查真实进程、任务和文件活动：

- `dist/ralph-run/status.md`：可读状态页（打开后手动刷新）。
- `dist/ralph-run/status.json`：进程、后端 PID、轮次、任务与最近活动时间。
- `dist/ralph-run/output.log`：增加 `[observer ...]` 状态行，原始循环输出仍保留。

观察器不发起额外 Codex 任务、不重启循环、不标记验收通过；进程退出时明确记录退出。
`task.done` 后短暂出现 `timeout_secs=0` 的警告还可能是 Ralph 的事件收尾路径，
应结合事件顺序和下一轮进程判断，不能单凭该行认定整个循环停止。

## 详细日志接续（2026-10-13）

原循环在第 3 轮收尾时因连续三次后端非正常退出停止，退出码 1；这不是验收完成。
2.10.1 的执行器看到 emit 的 CLI 确认文字后只留 5 秒退出，Codex 尚未正常结束便被终止，
随后被计入连续失败。已在 PROMPT.md 要求把 emit 的 stdout 追加到本地 emit-acks.log；
事件文件、错误输出和退出码保持原样，不提高失败上限或伪造成功。

接续使用相同 loop ID、`--continue --verbose`，保留已完成任务和所有未提交修改。
接续保留累计轮数，上限仍为 30 轮；本次运行时间上限为 13424 秒。详细 stderr、工具执行及结果写入 output.log，
会话另存 session-verbose-resume.jsonl。此前日志和 process.json 已备份保留。

实时追踪执行过程（持续更新，无需手动刷新）：

```sh
tail -n 80 -F /home/ubuntu/DshanControl/dist/ralph-run/output.log
```

文件链接的预览不会自动滚动；终端 tail 才会连续显示新增输出。

接续命令显式指定 `--backend codex -c dist/ralph-run/resume.yml`；该配置从 ralph.yml 复制，
仅扣除已消耗运行时间。不要只传一个字段覆盖参数，否则 2.10.1 会跳过默认项目配置。
一次错误配置的启动已在工具执行前终止，记录保留在日志，随后校正配置再启动。
