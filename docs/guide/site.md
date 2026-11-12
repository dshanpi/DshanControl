# 维护手册并发布 GitHub Pages

供文档维护人员使用。本站沿用同仓库 Astro/Starlight 结构，参考 [EinkOS 文档目录](https://github.com/dshanpi/EinkOS/tree/main/docs)，以中文任务导航、全文搜索和移动端目录发布 DshanControl 手册。

## 1. 找到正文与站点配置

| 路径 | 修改用途 |
| --- | --- |
| `docs/guide/*.md` | 手册正文，GitHub 与站点共用 |
| `docs/routes.mjs` | 页面来源、slug、标题与导航分组 |
| `docs/images/qt5-desktop/` | 有来源的界面图和架构图 |
| `docs/src/styles/custom.css` | 阅读布局与样式 |
| `docs/scripts/` | 正文同步、链接转换及产物检查 |
| `.github/workflows/docs.yml` | 构建与 GitHub Pages 发布 |

`docs/src/content/docs/`、`docs/public/images/` 和 `docs/dist/` 是生成目录，不直接编辑或提交。原始设计、板级资料和验证记录保留原位置。

## 2. 按操作流程编写正文

每篇围绕一个明确任务展开：

1. 开头说明适用环境、需要准备的输入及完成目标。
2. 标题使用具体动作，按实际执行顺序安排。
3. 每个关键步骤写明入口或工作目录、操作及预期结果。
4. 在需要决定是否继续的位置给出退出码、状态、文件或读回判据。
5. 将相关失败处理放在步骤附近，共用问题链接到排障页。

界面名称从当前源码或实测界面核对，命令先在对应环境验证。示例输出、主机渲染、软件测试和真实设备证据分别标明，不复述作者的调试经历。

## 3. 增加或修改页面

1. 在 `docs/guide/` 编辑 UTF-8 Markdown，以一个一级标题开始。
2. 新页面在 `routes.mjs` 登记 `source`、`slug`、`group` 和 `title`。
3. 用 `quickstart.md` 等相对路径连接教程，图片使用 `../images/qt5-desktop/...`。
4. 保持已有 URL 稳定；拆分主题时保留原入口并链接到新章节。
5. 检查标题、字段范围、命令目录、图片说明和正文是否一致。

引用未加入路由的源码或历史证据时，站点会将其转换为 GitHub 链接，教程之间则转换为站内地址。

## 4. 安装并构建站点

使用 Node.js **24 LTS**，与 CI 主版本一致。在**Ubuntu 主机、仓库根目录**执行：

```bash
node --version
cd docs
npm ci
npm run build
```

检查依赖安装、链接转换测试、Astro 构建、中文索引和静态链接/锚点/资源检查全部成功。失败时先修正文或路径，再进入预览。

## 5. 预览阅读效果

在 **`docs` 目录**执行：

```bash
npm run preview -- --host 127.0.0.1
```

访问终端显示的 `/DshanControl/` 地址。远程主机需要可达地址或端口转发；远端 `127.0.0.1` 不是当前电脑的本地地址。

开发时可使用 `npm run dev`，新增路由后重启服务。检查首页、长教程、表格和代码块，再验证中文搜索、子页直接访问、窄屏菜单和明暗主题。

## 6. 提交并发布

完成内容审校及仓库要求的检查，提交并推送文档变更到 `main`。工作流 “DshanControl documentation” 会安装锁定依赖、测试、构建、上传 Pages 产物并部署；PR 只构建，部署仅限 `main`。

仓库 Settings → Pages 的构建来源应为 GitHub Actions。需要手动重新部署当前 `main` 时，可运行该工作流。

## 7. 验证公开版本

1. 确认对应提交的 build 和 deploy 均成功。
2. 访问 [DshanControl 手册](https://dshanpi.github.io/DshanControl/)，核对新标题和正文。
3. 直接打开一个子页，检查刷新、图片、上一页/下一页与目录。
4. 用新正文中的中文词检索，打开结果并核对章节定位。
5. 保留发布提交、Actions 链接与检查记录。

配置文件中出现网址只代表目标地址；部署成功且公开访问正常后，才记录为发布完成。
