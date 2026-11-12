export const site = 'https://dshanpi.github.io';
export const base = '/DshanControl';
export const repository = 'https://github.com/dshanpi/DshanControl';
export const routes = [
  {
    "source": "docs/guide/index.md",
    "slug": "index",
    "group": "开始使用",
    "title": "认识 DshanControl"
  },
  {
    "source": "docs/guide/quickstart.md",
    "slug": "quickstart",
    "group": "开始使用",
    "title": "在 Ubuntu 上完成首次体验"
  },
  {
    "source": "docs/guide/hardware.md",
    "slug": "hardware",
    "group": "开始使用",
    "title": "确认硬件、系统与交付物"
  },
  {
    "source": "docs/guide/desktop.md",
    "slug": "desktop",
    "group": "桌面与业务操作",
    "title": "操作工作台、抽屉与搜索"
  },
  {
    "source": "docs/guide/widgets.md",
    "slug": "widgets",
    "group": "桌面与业务操作",
    "title": "编辑组件并保存工作台"
  },
  {
    "source": "docs/guide/fieldbus.md",
    "slug": "fieldbus",
    "group": "桌面与业务操作",
    "title": "使用 CAN、Modbus 与 EtherCAT 页面"
  },
  {
    "source": "docs/guide/network.md",
    "slug": "network",
    "group": "桌面与业务操作",
    "title": "检查有线、无线与蜂窝网络"
  },
  {
    "source": "docs/guide/monitoring.md",
    "slug": "monitoring",
    "group": "桌面与业务操作",
    "title": "查看趋势、点位与告警"
  },
  {
    "source": "docs/guide/cloud.md",
    "slug": "cloud",
    "group": "桌面与业务操作",
    "title": "完成云绑定与数据上报"
  },
  {
    "source": "docs/guide/home-voice.md",
    "slug": "home-voice",
    "group": "桌面与业务操作",
    "title": "使用家庭设备与语音入口"
  },
  {
    "source": "docs/guide/media.md",
    "slug": "media",
    "group": "桌面与业务操作",
    "title": "选择媒体并检查播放状态"
  },
  {
    "source": "docs/guide/settings.md",
    "slug": "settings",
    "group": "桌面与业务操作",
    "title": "调整显示、查看服务和日志"
  },
  {
    "source": "docs/guide/configuration.md",
    "slug": "configuration",
    "group": "维护与发布",
    "title": "管理配置、权限与数据备份"
  },
  {
    "source": "docs/guide/architecture.md",
    "slug": "architecture",
    "group": "开发与集成",
    "title": "理解系统分层与源码目录"
  },
  {
    "source": "docs/guide/development.md",
    "slug": "development",
    "group": "开发与集成",
    "title": "编译、测试并扩展应用"
  },
  {
    "source": "docs/guide/api.md",
    "slug": "api",
    "group": "开发与集成",
    "title": "理解本机 HTTP API 与 IPC"
  },
  {
    "source": "docs/guide/board-porting.md",
    "slug": "board-porting",
    "group": "开发与集成",
    "title": "构建 S4 用户态并接入新板卡"
  },
  {
    "source": "docs/guide/updates.md",
    "slug": "updates",
    "group": "维护与发布",
    "title": "区分应用更新、系统更新与烧录"
  },
  {
    "source": "docs/guide/troubleshooting.md",
    "slug": "troubleshooting",
    "group": "维护与发布",
    "title": "按现象定位常见问题"
  },
  {
    "source": "docs/guide/gallery.md",
    "slug": "gallery",
    "group": "开始使用",
    "title": "查看界面与学习入口"
  },
  {
    "source": "docs/guide/versions.md",
    "slug": "versions",
    "group": "维护与发布",
    "title": "核对版本、证据与已知限制"
  },
  {
    "source": "docs/guide/site.md",
    "slug": "site",
    "group": "维护与发布",
    "title": "维护教程并发布 GitHub Pages"
  }
];
export const sidebar = ['开始使用','桌面与业务操作','开发与集成','维护与发布'].map(label => ({label, items: routes.filter(p => p.group === label).map(p => ({label: p.title, slug: p.slug}))}));
