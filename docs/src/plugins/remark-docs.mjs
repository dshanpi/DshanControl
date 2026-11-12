import {visit} from 'unist-util-visit';
import {rewriteLink, sourceForGeneratedFile} from '../../source-links.mjs';
export function remarkDocs() {
  return (tree,file) => {
    const source=sourceForGeneratedFile(file.path);
    if (!source) return;
    visit(tree,node=>{
      if (['link','image','definition'].includes(node.type) && node.url) node.url=rewriteLink(node.url,source);
    });
  };
}
