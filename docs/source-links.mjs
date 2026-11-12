import path from 'node:path';
import { existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {base, repository, routes} from './routes.mjs';
const root = fileURLToPath(new URL('../',import.meta.url));
export function rewriteLink(url, source) {
  if (/^([a-z][a-z0-9+.-]*:|#|\/\/)/i.test(url)) return url;
  const [, pathname, suffix] = url.match(/^([^?#]*)(.*)$/);
  if (pathname.startsWith('/')) throw new Error('Use repository-relative links: '+url);
  const target = path.posix.normalize(path.posix.join(path.posix.dirname(source),decodeURIComponent(pathname)));
  if (target.startsWith('../') || path.posix.isAbsolute(target)) throw new Error('Link escapes repository: '+url);
  if (!existsSync(path.join(root,target))) throw new Error('Missing source: '+source+' -> '+url);
  const page = routes.find(item=>item.source===target);
  if (page) return base+'/'+(page.slug==='index'?'':page.slug+'/')+suffix;
  if (target.startsWith('docs/images/qt5-desktop/')) return base+'/images/qt5-desktop/'+target.slice('docs/images/qt5-desktop/'.length)+suffix;
  return repository+'/blob/main/'+target.split('/').map(encodeURIComponent).join('/')+suffix;
}
export function sourceForGeneratedFile(file) {
  const slug=String(file).replaceAll('\\','/').split('/src/content/docs/')[1]?.replace(/\.md$/,'');
  return routes.find(item=>item.slug===slug)?.source;
}
