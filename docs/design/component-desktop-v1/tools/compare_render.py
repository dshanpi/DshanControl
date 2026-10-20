#!/usr/bin/env python3
"""Exact RGBA comparison. Any changed pixel fails; no permissive threshold."""
import argparse,json,sys
from pathlib import Path
from PIL import Image,ImageChops
p=argparse.ArgumentParser();p.add_argument('reference');p.add_argument('candidate');p.add_argument('--report');a=p.parse_args()
reference=Path(a.reference).resolve(); candidate=Path(a.candidate).resolve()
if reference==candidate:p.error('Reference and candidate must differ')
rows=[]
for src in sorted(reference.glob('UI新桌面_*.png')):
    row={'file':src.name,'pass':False}; dst=candidate/src.name
    if not dst.is_file():row['error']='missing';rows.append(row);continue
    with Image.open(src) as im1,Image.open(dst) as im2:
        if im1.size!=im2.size:row['error']='size mismatch';rows.append(row);continue
        diff=ImageChops.difference(im1.convert('RGBA'),im2.convert('RGBA'))
        channels=diff.split(); mask=channels[0]
        for ch in channels[1:]:mask=ImageChops.lighter(mask,ch)
        histogram=mask.histogram(); changed=sum(histogram[1:])
        row.update(changed_pixels=changed,total_pixels=im1.width*im1.height,max_channel_delta=max(x[1] for x in diff.getextrema()),bbox=mask.getbbox())
        row['pass']=changed==0
    rows.append(row)
result={'pass':len(rows)==10 and all(x['pass'] for x in rows),'count':len(rows),'comparison':'exact RGBA; no masks or thresholds','images':rows}
text=json.dumps(result,ensure_ascii=False,indent=2)
if a.report:
    target=Path(a.report).resolve()
    if target==reference or reference in target.parents:p.error('Report must not modify reference directory')
    target.write_text(text+'\n',encoding='utf-8')
print(text);sys.exit(0 if result['pass'] else 1)
