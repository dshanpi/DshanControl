# 视频播放器 Qt 设计

用户在2026-10-16新增：云服务、MQTT、涂鸦对话和智能家庭之后，实现调用板端编解码
能力的视频播放器；先交UI渲染图。此前云服务落实授权继续有效，尚未完成，不能因
播放器预览而把官网/语音/家庭联调标为通过。本轮先完成播放器视觉预览供用户查看。

## 交付位置

遵守终端图片门禁：实际PNG复制到 `/home/ubuntu/T113-tina5v1.2-sdk/`，
不以聊天图片、另一仓库路径或HTTP服务代替交付，不公开上传。

- `UI播放器总览.png`
- `UI播放器播放.png`
- `UI播放器本地视频.png`
- `UI播放器设置.png`
- `UI播放器错误状态.png`
- `UI播放器未选文件.png`
- `UI播放器暂停.png`
- `UI播放器全屏.png`
- `UI播放器800x480.png`
- `UI播放器1280x720.png`

原始构建/图像/日志：`dist/video-player-design-20261016/`。
源码：`tools/design/video_player_preview.cpp` 与 `.pro`。

## 视觉与交互

沿用工业桌面白色工作面、浅蓝导航#E8F3FF、背景#F2F7FD、主蓝#0062FF、
正文#092650、边框#D9E3EF与内置Dshan Sans。仅视频显示区用深色#172535，
保留视频比例，所有控制位于白色工具栏。无装饰动画、泛化聊天/营销布局。

1024×600：40px顶栏、140px左导航、28px状态栏；工作区上方标题与页内入口，
左侧视频和固定播放控制、右侧228px播放列表。800×480收起播放列表，通过页顶
“本地视频”切换，导航改为68px短标签并允许独立滚动，不缩小触控目标。
全屏收起桌面导航与顶栏，保留进度、声音和退出按钮。

播放/暂停、上下视频、进度拖动、音量、静音、列表选择和全屏在原型中演示状态
切换，不读取真实存储、不打开音频/视频设备。UI由Qt真实排版并截图，没有图片
生成模型的文字或控件。画面是代码绘制的接线示意占位，不是已解码的视频或实物照片。
示例文件、时长、H.264和1280×720只描述设计数据，不宣称板端支持或实测性能。
硬解、实际帧率和丢帧保持“待验证/--”，解码失败不无限重试。

## 后续原生播放器接入

本地SDK已核对的来源：

- `platform/allwinner/multimedia/libtmedia/tplayer/tplayer.h`
- `platform/allwinner/multimedia/libcedarc/include/vdecoder.h`
- `platform/allwinner/multimedia/multimedia_demo/tplayerdemo/src/tplayerdemo.c`
- `platform/allwinner/multimedia/multimedia_demo/tplayerdemo/src/Makefile`

当前S4清理构建配置存在 `BR2_PACKAGE_LIBCEDARX=y`、`LIBCEDARC=y`；目标目录
有 `libtplayer.so`、`libxplayer.so`、`libvdecoder.so`。这是构建输入/文件存在证据，
尚未核对当前板端实际加载链、权限、显示层与录放音，不能视为硬件解码已运行。

实现顺序：

1. 独立C++播放器适配层封装 `TPlayerCreate(CEDARX_PLAYER)`、回调、
   `TPlayerSetDataSource`、`TPlayerPrepareAsync`、`TPlayerStart/Pause/SeekTo`。
   以PREPARED、首画面、SEEK_COMPLETE、BUFFER、ERROR和COMPLETE实际事件更新UI；
   SDK回调线程通过Qt队列回到UI线程。用户只选文件即失败时不自动重发播放操作。
2. 核对S4 RGB显示输出、显示层矩形/比例、Qt framebuffer图层与视频层合成；
   核对Cedar/VE/内存与音频设备权限，不把root或任意设备访问权给桌面。
3. 文件列表只扫描获准的媒体目录和实际已挂载USB；检查访问性与移除情况，
   文件名/元数据按纯文本呈现，路径作为API参数传递，不拼接shell命令。
4. 解码器身份、尺寸、时长和帧率来自实际媒体信息/解码结果；支持格式根据
   当前构建和实际样片验证，不将头文件枚举当支持列表，不默认软件回退。
5. 与涂鸦语音服务协调音频占用/音量，退出播放器时释放视频层、VE和音频资源。
6. 按现有系统/应用边界构建：Cedar/驱动/基础库属于系统；Qt UI与适配程序属于
   应用。若需更改设备权限或系统启动政策，明确记录并单独验证。

播放器首先完成解码、同步与播放；硬件编码/录制/转码是另一条数据链，需核对输入、
编码库与真实能力后接入，不能以播放器UI或解码成功冒充编码已经可用。

## 重建

```sh
mkdir -p dist/video-player-design-20261016/qt-host
qmake tools/design/video_player_preview.pro -o dist/video-player-design-20261016/qt-host/Makefile
make -C dist/video-player-design-20261016/qt-host -j2
QT_QPA_PLATFORM=offscreen dist/video-player-design-20261016/qt-host/video-player-preview \
  --render dist/video-player-design-20261016/screenshots
```

渲染过程同时检查暂停/定位交互、真实窗口尺寸及可见按钮至少44px，生成四页总览
和9张原始截图。构建、回归、图像及复制哈希记录在同目录 `validation.json`。
不改变现有板端应用、用户数据、系统镜像、签名或更新状态。

实际结果：主机Qt5.12.8、S4 Qt5.11.3预览构建通过，无编译警告；ARM ELF32 /
soft-float ABI已核对。暂停/进度定位、实际退出全屏、可见按钮至少44px与工作区
按钮边界检查通过。9张原始图尺寸均核对，另有2096×1328总览，10张图已复制到
SDK根目录并逐个SHA256对照。首轮800px窗口高度552的拒绝日志保留，修正为
真实800×480，未缩放图片冒充适配。`make test`通过核心5项、仓库247项，
其中9项既有条件跳过。当前仍无真实视频、硬解、硬编或音频运行证据。
