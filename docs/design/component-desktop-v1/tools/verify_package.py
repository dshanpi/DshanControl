#!/usr/bin/env python3
"""Read-only verification. No network, hardware calls or product writes."""
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
errors=[]
manifest=json.loads((ROOT/'manifest.sha256.json').read_text(encoding='utf-8'))
for item in manifest['files']:
    p=ROOT/item['path']
    if not p.is_file():errors.append('missing: '+item['path']);continue
    if hashlib.sha256(p.read_bytes()).hexdigest()!=item['sha256']:errors.append('hash: '+item['path'])
    if p.suffix=='.json':
        try:json.loads(p.read_text(encoding='utf-8'))
        except Exception as e:errors.append('json: '+item['path']+' '+str(e))
layout=json.loads((ROOT/'contracts/default-layout.json').read_text(encoding='utf-8'))
compat=json.loads((ROOT/'contracts/widget-compatibility.json').read_text(encoding='utf-8'))['slots']
ids=set(); pageids=set()
for page in layout['pages']:
    if page['id'] in pageids:errors.append('duplicate page')
    pageids.add(page['id'])
    if {x['slot_id'] for x in page['slots']}!=set(compat):errors.append('wrong slots')
    for item in page['slots']:
        if item['id'] in ids:errors.append('duplicate widget')
        ids.add(item['id'])
        if item['type']!='empty' and item['type'] not in compat[item['slot_id']]:errors.append('incompatible slot')
if layout['active_page'] not in pageids:errors.append('active page missing')
apps=json.loads((ROOT/'contracts/app-registry.json').read_text(encoding='utf-8'))['apps']
if len(apps)!=12 or len({x['id'] for x in apps})!=12:errors.append('app registry invalid')
from PIL import Image
pngs=list((ROOT/'reference/screens').glob('*.png'))
if len(pngs)!=10:errors.append('expected ten approved PNGs')
for p in pngs:
    expected=(2136,2028) if '_00' in p.name else (1280,720) if '_07' in p.name else (1024,600)
    with Image.open(p) as im:
        if im.size!=expected:errors.append('dimensions: '+p.name)
print(json.dumps({'ok':not errors,'checked_files':len(manifest['files']),'approved_pngs':len(pngs),'errors':errors},ensure_ascii=False,indent=2))
sys.exit(1 if errors else 0)
