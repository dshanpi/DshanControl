import { readFileSync, writeFileSync, mkdirSync, cpSync, readdirSync, unlinkSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { routes, repository } from '../routes.mjs';
const docsRoot = fileURLToPath(new URL('../', import.meta.url));
const root = path.dirname(docsRoot);
const output = path.join(docsRoot, 'src/content/docs');
export const sourcePaths = routes.map(page => path.join(root, page.source));
export function syncContent() {
  mkdirSync(output, {recursive:true});
  const names = new Set(routes.map(page => page.slug + '.md'));
  for (const name of readdirSync(output)) {
    if (name.endsWith('.md') && !names.has(name)) unlinkSync(path.join(output,name));
  }
  for (const page of routes) {
    const text = readFileSync(path.join(root,page.source),'utf8').replace(/\r\n/g,'\n');
    const heading = text.match(/^# ([^\n]+)\n/);
    if (!heading) throw new Error('Expected leading H1: ' + page.source);
    const result = '---\ntitle: ' + JSON.stringify(page.title) + '\neditUrl: ' +
      JSON.stringify(repository+'/edit/main/'+page.source) + '\n---\n' + text.slice(heading[0].length);
    const target = path.join(output,page.slug+'.md');
    let previous; try { previous = readFileSync(target,'utf8'); } catch {}
    if (previous !== result) writeFileSync(target,result);
  }
}
export function prepare() {
  syncContent();
  cpSync(path.join(docsRoot,'images/qt5-desktop'),path.join(docsRoot,'public/images/qt5-desktop'),{recursive:true});
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) prepare();
