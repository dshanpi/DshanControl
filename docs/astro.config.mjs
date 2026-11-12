import { defineConfig } from 'astro/config';
import { unified } from '@astrojs/markdown-remark';
import starlight from '@astrojs/starlight';
import { remarkDocs } from './src/plugins/remark-docs.mjs';
import { syncContent, sourcePaths } from './scripts/prepare-docs.mjs';
import { site, base, repository, sidebar } from './routes.mjs';
export default defineConfig({
  site, base, trailingSlash: 'always',
  markdown: { processor: unified({ remarkPlugins: [remarkDocs] }) },
  integrations: [{
    name: 'dshancontrol-canonical-guides',
    hooks: { 'astro:server:setup': ({server}) => {
      server.watcher.add(sourcePaths);
      const update = file => { if (sourcePaths.includes(file)) syncContent(); };
      server.watcher.on('change', update); server.watcher.on('add', update);
    } },
  }, starlight({
    title: 'DshanControl',
    description: 'Qt5 工业组件桌面详细教程：Ubuntu 体验、工作台、现场总线、云与家庭、开发集成及维护。',
    locales: { root: { label: '简体中文', lang: 'zh-CN' } },
    social: [{ icon: 'github', label: 'GitHub', href: repository }],
    sidebar, customCss: ['./src/styles/custom.css'],
    head: [{tag:'meta', attrs:{name:'theme-color',content:'#1766d5'}}],
  })],
});
