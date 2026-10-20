# Qt5 组件桌面测试版（2026-10-20）

**本分支是源码与设计需求预览，不是生产正式版，也不代表全部需求已验收。**

实现顶部信息栏与下拉控制中心、中间可配置组件桌面、底部上拉应用抽屉，并接入组件编辑、图表、图片、媒体、云服务与家庭/语音服务界面。各项真实业务接入和硬件验证的完成程度以下方验收表为准。

## 设计与实现入口

- [一比一设计需求包](docs/design/component-desktop-v1/README.md)：原始批准PNG、几何/颜色/布局合同、操作流程、业务规则、接口调用、架构任务与原始验收清单。原包47个文件逐字节保留；其中10张批准PNG不作修改。
- [当前73项验收进度](docs/validation/component-desktop-preview-acceptance.csv)：25 PASS、47 PARTIAL、1 RUNNING。Qt测试用例数量不等于需求验收数量。
- `components/hmi/src/desktop/`：Qt桌面绘制、布局、组件编辑、点位趋势、图片、输入与应用入口。
- `components/desktop/`：后台数据与云/家庭/语音协议边界；凭据由后台读取，不能提交到仓库。
- `components/hmi/src/{cloudpanel,familypanel,mediapanel,desktopclient}.*`：业务界面与IPC客户端。
- `components/runtime/`、`components/media/`：原生进程、权限、生命周期与媒体适配。
- `tests/hmi/`、`tests/test_desktop_service.py`、`tests/test_desktop_voice_lifecycle.py`：界面、真实私有IPC/TLS MQTT及服务边界回归。

## 已有验证及边界

|项目|实际结果|边界|
|---|---|---|
|批准效果图|10张RGBA逐像素差异为0|固定参考场景；不替代实际LCD颜色/触摸验收|
|Qt主机回归|87主回归＋6组件隔离＋15上报页面全部通过|Ubuntu Qt5.12.8 offscreen|
|r23实际ARM回归|87主回归＋6组件隔离全部通过，恢复后4次原生健康、布局和安装身份保持|ARM Qt5.11.3隔离offscreen；新增15上报用例仅完成ARM构建|
|上报服务边界|无数据/过期拒发、序号防重、关闭上报仍采集、配置跨进程重启保持等私有TLS/IPC测试通过|真实云端账号与入库未验收；PUBACK只代表Broker确认|
|语音取消与超时|Qt取消/失活/隐藏/迟到释放场景，以及私有桥30/120秒超时通过|不代表实际麦克风、扬声器与授权语音服务通过|
|r23长测|截至北京时间2026-10-20 18:29，只读进度报告1000次切页，连续观察3043.359秒/99样本|尚未完成86400秒；原始逐条记录和内存趋势仍待终态核验|

r23板端HMI SHA256：`9c115d834ef09d5de76b173d60d178a1f3cf332abc444e6ba55452789128e86f`。
这是现有板端候选身份，不表示从任意主机重编译都会产生相同文件。发布整理期间未部署、重启或停止该候选。最近一次真机通道查询受到正在进行的硬件任务占用限制，因此上述时间是最后已确认的观测，不把旧进度称为当前实测。

## 本次发布副本复核

从独立Git源码导出副本重新构建：核心5项通过；仓库289项中279通过、10项因环境条件跳过；Qt界面87＋6＋15全部通过；需求包46个清单文件及10张批准图校验通过。首次发现深目录下语音测试Unix socket路径过长，已修正测试临时路径并完整重跑；生产桌面代码未因此修改。详见[本次验证摘要](docs/validation/2026-10-20-preview-host-validation.json)。

## 未验收项目（继续保留）

1. 同一启动、同一进程下完成真实86400秒观察，并取回1000次切页、心跳、内存/CPU和失败原始记录进行人工评审。
2. 物理LCD颜色/比例、真实触摸、键盘/滚动与各业务操作完整流程。
3. 真实云端、家庭设备授权及语音服务端到端；S4实际录音与播放。
4. 原生UID402后台服务角色及对应系统契约迁移；当前板端仍是旧root且无capability的helper。
5. 其余工业接口、媒体和启动校时可靠性，见验收CSV；一次NTP成功不能证明所有启动可靠。
6. 生产签名、固件、整板刷写及自动更新资格不在本次源码测试版交付范围内。

r20与r21的中断记录保留，不能合并为一次24小时成功；r21的11646.044秒观察不属于r23结果。原始实验记录、私密配置与本机恢复入口保留在SDK本地，不随源码公开上传。

## Ubuntu开发与回归

使用Linux、Qt5开发工具（Core/Gui/Widgets/Network/Test及qmake）、C++编译器、OpenSSL/zlib开发包；Python隔离依赖见`tests/requirements.txt`。私有TLS/MQTT测试还需要`openssl`、`mosquitto`、`mosquitto_pub`和`mosquitto_sub`。部分权限测试要求root，未满足条件会明确跳过。Unix通信测试使用系统临时目录；如需指定`TMPDIR`，请选择较短的路径，避免Linux Unix socket长度限制。

```sh
make test-bootstrap
make test HMI_QMAKE=/usr/bin/qmake
make test-hmi HMI_QMAKE=/usr/bin/qmake
```

界面运行时需要本地Web API和后台IPC服务；预览/测试场景不能当作真实业务服务。板端目标为T113s4 SD NAND/Qt5.11.3，需要匹配的合法SDK与系统契约。`components/imageformats/`固定Qt5.11.3私有ABI，媒体硬件解码需要目标SDK的Cedar接口；它们不是通用PC库，不要替换系统Qt插件。

需求包中的README保留原交付地址作为历史出处；GitHub检出后从`docs/design/component-desktop-v1/`访问即可。其`tools/verify_package.py`可核验原包清单，需要Python Pillow（Ubuntu的`python3-pil`）。第三方拼音与图像编解码源码的来源和许可随各组件保留。本次没有携带设备密钥、用户数据、私有SDK或厂商预编译库。
