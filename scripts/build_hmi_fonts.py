#!/usr/bin/env python3
"""Rebuild bundled OFL font subsets from Debian's Noto CJK 20190410 TTC files."""
import argparse
import hashlib
import json
from pathlib import Path
import fontTools
from fontTools import subset
from fontTools.ttLib import TTCollection

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=Path('/usr/share/fonts/opentype/noto'))
parser.add_argument('--output', type=Path, help='Isolated output directory for reproducibility checks')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = args.output or root / 'components/hmi/src/fonts'
out.mkdir(parents=True, exist_ok=True)
characters = set(chr(i) for i in range(32, 127))
for high in range(0xa1, 0xf8):
    for low in range(0xa1, 0xff):
        try:
            characters.update(bytes([high, low]).decode('gb2312'))
        except UnicodeDecodeError:
            pass
characters.update('°℃›●▶▼…–—✓×·▣')
records = []
for weight in ('Regular', 'Bold'):
    source = args.source / ('NotoSansCJK-' + weight + '.ttc')
    font = TTCollection(str(source)).fonts[2]
    font.recalcTimestamp = False
    options = subset.Options()
    options.name_IDs = ['*']
    options.name_languages = ['*']
    sub = subset.Subsetter(options=options)
    sub.populate(text=''.join(sorted(characters)))
    sub.subset(font)
    names = {1:'Dshan Sans', 2:weight, 3:'DshanSans-' + weight,
             4:'Dshan Sans ' + weight, 6:'DshanSans-' + weight,
             16:'Dshan Sans', 17:weight}
    for record in font['name'].names:
        if record.nameID in names:
            record.string = names[record.nameID].encode(record.getEncoding())
    cff = font['CFF '].cff
    cff.fontNames = ['DshanSans-' + weight]
    cff.topDictIndex[0].FamilyName = 'Dshan Sans'
    cff.topDictIndex[0].FullName = 'Dshan Sans ' + weight
    target = out / ('DshanSans-' + weight + '.otf')
    font.save(str(target))
    records.append({'source':source.name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
                    'output':target.name,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
license_bytes = (root / 'components/hmi/src/fonts/OFL.txt').read_bytes()
(out / 'OFL.txt').write_bytes(license_bytes)
(out / 'manifest.json').write_text(json.dumps({'family':'Dshan Sans','coverage':'GB2312, Latin and UI symbols',
    'license_sha256':hashlib.sha256(license_bytes).hexdigest(),
    'origin':'Noto Sans CJK SC','fonttools_version':fontTools.__version__,'license':'SIL Open Font License 1.1','fonts':records},indent=2)+'\n')
