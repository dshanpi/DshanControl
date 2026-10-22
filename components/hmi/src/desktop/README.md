# Qt5 应用框架

组件桌面以 `DesktopShell` 管理工作台、遮罩和应用容器，`MainWindow` 承载业务页面。注册信息来自编译进资源的 `assets/app-registry.json`，不是插件执行清单；不会运行其中的命令、加载任意动态库或赋予后台能力。

## 注册与路由

`applicationregistry.h` 集中解析、缓存并校验注册信息，按显式 `order` 排序；抽屉分类取自 `category`，组件点击通过稳定应用 ID 查找。无效或重复 ID、无处理器的入口、重复顺序和不合法字段在注册阶段拒绝，不部分覆盖原清单。

`ApplicationRegistry::resolve()` 先生成经过校验的路由，再由 `MainWindow::openApplication()` 切页。失败返回 `false`，原页面不变。注册 ID 和页面映射如下：

|ID|页面/子页|允许的上下文|
|---|---|---|
|overview|运行总览|无|
|fieldbus|现场总线|无|
|network|网络通信|无|
|devices|设备/趋势|point_id、secondary_point_id，均为 device/point|
|alarms|告警记录|无|
|settings|系统设置|无|
|cloud_bind|云绑定|无|
|reporting|数据上报|无|
|voice|语音|无|
|home|家庭|device_id|
|video|媒体|path，规范化的本地绝对路径|
|update|系统更新|无|

上下文禁止多余键、非字符串、NUL/换行和越界标识符。路由只保留明确绑定，不发现设备、不打开媒体文件、不发送控制请求。实际文件权限、后台能力和业务授权仍由相应服务检查。

`DesktopShell::activeApplication` 及 `activeApplicationChanged` 只在成功切换应用后改变；返回工作台清空，打开遮罩保持。相同 ID 再次进入不会重复发出状态变化信号。

## 添加应用

1. 在注册资源中添加稳定 ID、标题、图标、颜色、分类和唯一顺序。
2. 在注册器中添加明确页面处理器；确需上下文时增加对应字段约束。
3. 在 `MainWindow::openApplication()` 接入业务页面及其后台服务，保留后台权限检查。
4. 增加注册/上下文拒绝、页面进入和返回测试。更改视觉时另行核对批准图，不能修改原图掩盖差异。

## 主机开发

需要 Qt5 Core/Gui/Widgets/Network/Test、qmake、C++11 编译器、OpenSSL/zlib 开发包。完整仓库依赖与验证入口见根目录 `PREVIEW.md`。

```sh
mkdir -p dist/hmi
cd dist/hmi
/usr/bin/qmake ../../components/hmi/src/omnigate-hmi.pro
make
QT_QPA_PLATFORM=offscreen ./omnigate-hmi --desktop-render ../desktop-render
```

`--desktop-render` 是隔离示例渲染，不能当作实时设备、云服务或实板运行证据。桌面正常运行使用 `--windowed`；未连接本机 API/IPC 时显示不可用状态，不回填示例数据。后台服务接口、私密配置与权限归属见 `components/desktop/` 和系统集成文档。

```sh
make test-application-registry HMI_QMAKE=/usr/bin/qmake
make test-hmi HMI_QMAKE=/usr/bin/qmake
```

## 源码交付

`make package` 只从干净的已提交 Git 树导出，忽略未跟踪/忽略文件；压缩时间固定，随包提供 SHA256 和源提交元数据。未提交或暂存改动会拒绝导出，已有同名归档不被覆盖。该包不是应用更新包或固件，不包含安装资格声明。
