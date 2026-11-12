# 维护教程并发布 GitHub Pages

本站参考 [EinkOS docs](https://github.com/dshanpi/EinkOS/tree/main/docs) 的同仓库 Astro/Starlight 结构，使用中文分组导航、全文搜索、浅色/深色主题和移动端目录。内容针对 DshanControl 的公开源码重写。

## 目录结构

| 路径 | 用途 |
| --- | --- |
| `docs/guide/*.md` | 教程正文，GitHub 和文档站共用 |
| `docs/routes.mjs` | 页面来源、slug、分组和标题 |
| `docs/images/qt5-desktop/` | 已有主机渲染与架构图，保留来源 |
| `docs/src/content/docs/` | 构建生成页面，不提交 |
| `docs/public/images/` | 构建复制的站点资源，不提交 |
| `docs/scripts/` | 同步、链接转换测试与产物检查 |
| `docs/dist/` | 静态网站产物，不提交 |
| `.github/workflows/docs.yml` | 自动构建和 Pages 部署 |

原有设计、历史验收与板级说明保留原位置。正文引用未加入站点路由的源码/证据时，会链接到 GitHub；它们不会被伪装成新的教程页面。

## 本地安装和预览

使用 Node.js **24 LTS**，本地与 CI 保持同一主版本。在仓库 `docs` 目录执行：

```bash
npm ci
npm run build
npm run preview -- --host 127.0.0.1
```

访问终端显示的 `/DshanControl/` 地址。远程主机预览需要可达的主机地址或受支持的端口转发，不能把远端 `127.0.0.1` 当成本机。

开发模式使用：

```bash
npm run dev
```

正文变化会同步到生成目录；新增路由后重启开发服务。

## 增加一篇教程

1. 在 `docs/guide/` 添加 UTF-8 Markdown，以一个一级标题开头。
2. 在 `routes.mjs` 登记 source、slug、group 和 title。
3. 使用相对链接引用其他教程，例如 `quickstart.md`；图片使用 `../images/qt5-desktop/...`。
4. 每篇写清执行位置、前提、操作步骤、成功判据和必要异常处理。
5. 运行 `npm run build`，核对内部路径、锚点、资源和中文搜索。

不要编辑 `src/content/docs` 的生成副本。既有 URL 尽量稳定，避免从外部链接进来的读者遇到 404。

## 发布流程

仓库 Pages 使用 GitHub Actions。`main` 上的文档变更会触发 “DshanControl documentation”：安装固定依赖 → 测试链接转换 → 构建 → 检查静态产物 → 上传 Pages artifact → 部署。

PR 只构建验证；部署限定 `main`。手动运行 workflow 也可重新部署当前 `main`。需要首次配置时，在仓库 Settings → Pages 选择 GitHub Actions。

预期地址为 [DshanControl 教程](https://dshanpi.github.io/DshanControl/)。必须看到 deploy 成功并实际访问页面，才能报告“已发布”；仅有配置中的 URL 不够。

## 发布后的复核

检查首页、至少一篇操作教程与开发页、图片、目录、上一页/下一页、中文搜索、窄屏菜单和主题切换。直接打开一个子页 URL，确认刷新不会 404。保留提交和 Actions 链接。

站点依赖按锁文件安装，Astro/Starlight 及其他依赖遵守各自许可。DshanControl 原有许可证、字体 OFL 和第三方资料不因新增文档改变。
