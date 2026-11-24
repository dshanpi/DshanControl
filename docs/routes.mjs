export const site = 'https://dshanpi.github.io';
export const base = '/DshanControl';
export const repository = 'https://github.com/dshanpi/DshanControl';
export const routes = [
  {
    "source": "docs/guide/index.md",
    "slug": "index",
    "group": "阅读与准备",
    "title": "开始使用 DshanControl"
  },
  {
    "source": "docs/guide/hardware.md",
    "slug": "hardware",
    "group": "阅读与准备",
    "title": "核对硬件、系统与安装文件"
  },
  {
    "source": "docs/guide/versions.md",
    "slug": "versions",
    "group": "阅读与准备",
    "title": "确认手册版本与功能适用范围"
  },
  {
    "source": "docs/guide/gallery.md",
    "slug": "gallery",
    "group": "阅读与准备",
    "title": "对照界面图定位操作入口"
  },
  {
    "source": "docs/guide/quickstart.md",
    "slug": "quickstart",
    "group": "主机体验",
    "title": "在 Ubuntu 编译并运行桌面"
  },
  {
    "source": "docs/guide/desktop-service.md",
    "slug": "desktop-service",
    "group": "主机体验",
    "title": "在主机启动并检查接入服务"
  },
  {
    "source": "docs/guide/desktop.md",
    "slug": "desktop",
    "group": "桌面与数据操作",
    "title": "打开应用并使用桌面导航"
  },
  {
    "source": "docs/guide/widgets.md",
    "slug": "widgets",
    "group": "桌面与数据操作",
    "title": "编辑组件并验证工作台保存"
  },
  {
    "source": "docs/guide/monitoring.md",
    "slug": "monitoring",
    "group": "桌面与数据操作",
    "title": "检查读数来源并查看趋势"
  },
  {
    "source": "docs/guide/alarms.md",
    "slug": "alarms",
    "group": "桌面与数据操作",
    "title": "筛选、确认并导出告警"
  },
  {
    "source": "docs/guide/fieldbus.md",
    "slug": "fieldbus",
    "group": "业务接入",
    "title": "准备现场总线联调"
  },
  {
    "source": "docs/guide/can.md",
    "slug": "can",
    "group": "业务接入",
    "title": "配置 CAN 链路并操作 CANopen"
  },
  {
    "source": "docs/guide/modbus.md",
    "slug": "modbus",
    "group": "业务接入",
    "title": "保存 RS485 参数并核对 Modbus 采集"
  },
  {
    "source": "docs/guide/ethercat.md",
    "slug": "ethercat",
    "group": "业务接入",
    "title": "检查 EtherCAT 映射并执行诊断"
  },
  {
    "source": "docs/guide/network.md",
    "slug": "network",
    "group": "业务接入",
    "title": "检查网络接口并恢复业务连接"
  },
  {
    "source": "docs/guide/cloud.md",
    "slug": "cloud",
    "group": "业务接入",
    "title": "完成官网绑定并验证数据上报"
  },
  {
    "source": "docs/guide/home-voice.md",
    "slug": "home-voice",
    "group": "业务接入",
    "title": "同步家庭设备并使用语音对话"
  },
  {
    "source": "docs/guide/media.md",
    "slug": "media",
    "group": "业务接入",
    "title": "选择本地视频并检查播放结果"
  },
  {
    "source": "docs/guide/settings.md",
    "slug": "settings",
    "group": "维护与验收",
    "title": "保存显示设置并检查系统服务"
  },
  {
    "source": "docs/guide/configuration.md",
    "slug": "configuration",
    "group": "维护与验收",
    "title": "部署接入配置并备份用户数据"
  },
  {
    "source": "docs/guide/updates.md",
    "slug": "updates",
    "group": "维护与验收",
    "title": "选择更新方式并检查安装结果"
  },
  {
    "source": "docs/guide/troubleshooting.md",
    "slug": "troubleshooting",
    "group": "维护与验收",
    "title": "按故障现象定位问题"
  },
  {
    "source": "docs/guide/acceptance.md",
    "slug": "acceptance",
    "group": "维护与验收",
    "title": "记录功能检查与验收结果"
  },
  {
    "source": "docs/guide/architecture.md",
    "slug": "architecture",
    "group": "开发与集成",
    "title": "定位系统模块与请求链路"
  },
  {
    "source": "docs/guide/development.md",
    "slug": "development",
    "group": "开发与集成",
    "title": "建立编译、测试与源码交付流程"
  },
  {
    "source": "docs/guide/api.md",
    "slug": "api",
    "group": "开发与集成",
    "title": "调用本机查询接口并处理异步结果"
  },
  {
    "source": "docs/guide/board-porting.md",
    "slug": "board-porting",
    "group": "开发与集成",
    "title": "构建 S4 用户态并规划板级移植"
  },
  {
    "source": "docs/guide/imx6ull-services.md",
    "slug": "imx6ull-services",
    "group": "开发与集成",
    "title": "适配 i.MX6ULL 总线、媒体与应用升级"
  },
  {
    "source": "docs/guide/site.md",
    "slug": "site",
    "group": "开发与集成",
    "title": "维护手册并发布 GitHub Pages"
  }
];
export const sidebar = ["阅读与准备", "主机体验", "桌面与数据操作", "业务接入", "维护与验收", "开发与集成"].map(label => ({label, items: routes.filter(p => p.group === label).map(p => ({label: p.title, slug: p.slug}))}));
