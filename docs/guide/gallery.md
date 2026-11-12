# 对照界面图定位操作入口

本页使用公开仓库中的 Qt5 桌面固定示例渲染，帮助定位操作区域。图中读数、设备名称和在线状态是示例，不代表当前连接状态；具体按钮与业务行为以对应版本实际界面为准。

## 从工作台进入应用

![工作台：组件集中显示常用信息](../images/qt5-desktop/workbench.png)

检查顶部状态区、中央组件和底部入口。首次操作按[桌面导航](desktop.md)完成打开抽屉、进入应用和返回。

![全部应用：按分类寻找功能入口](../images/qt5-desktop/app-drawer.png)

应用入口由注册清单提供。选择入口后，先查看业务状态，再执行控制或保存。

## 打开快捷控制与搜索

![控制中心：查看状态与快捷控制](../images/qt5-desktop/control-center.png)

状态信息需要对应后端支持。亮度相关检查见[系统设置](settings.md)。

![搜索：输入名称后选择匹配应用](../images/qt5-desktop/app-search.png)

没有结果时先清空输入，确认名称与注册入口一致。

## 编辑并保存组件

![编辑工作台：选中组件后修改，最后保存整页](../images/qt5-desktop/widget-editor.png)

![组件库：按兼容槽位选择类型](../images/qt5-desktop/widget-library.png)

组件设置先“应用到草稿”，再“完成编辑”。完整保存与取消流程见[工作台编辑](widgets.md)。

## 检查图片、展开与宽屏

![图片组件：确认本地文件和显示效果](../images/qt5-desktop/image-widget.png)

图片文件属于用户数据，移动或删除后需重新选择路径。

![组件展开：检查内容和关联业务入口](../images/qt5-desktop/expanded-widget.png)

![1280×720 宽屏固定示例：核对组件与边缘布局](../images/qt5-desktop/wide-screen.png)

宽屏图用于布局比较，其他分辨率与真实屏幕仍需分别检查。生成方法见[编译与界面审查](development.md)，原始设计来源见[组件桌面设计](../design/component-desktop-v1/README.md)。
