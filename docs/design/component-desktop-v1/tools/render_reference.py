#!/usr/bin/env python3
"""Build/render approved host Qt5 reference. Does not build or deploy production HMI."""
import argparse,os,shutil,subprocess,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',default='/tmp/dshan-desktop-reference');p.add_argument('--qmake',default='qmake');a=p.parse_args()
output=Path(a.output).resolve()
if output==ROOT or ROOT in output.parents or output in ROOT.parents:p.error('Output must be outside requirement package')
src=output/'source'; build=output/'build'; renders=output/'renders'
for d in (src,build,renders):d.mkdir(parents=True,exist_ok=True)
source=(ROOT/'reference/source/desktop-preview.cpp').read_text(encoding='utf-8')
mapping={
 '/home/ubuntu/T113-tina5v1.2-sdk/UI播放器1280x720.png':ROOT/'assets/media/UI播放器1280x720.png',
 '/home/ubuntu/DshanControl/components/hmi/src/fonts/DshanSans-Regular.otf':ROOT/'assets/fonts/DshanSans-Regular.otf',
 '/home/ubuntu/DshanControl/components/hmi/src/fonts/DshanSans-Bold.otf':ROOT/'assets/fonts/DshanSans-Bold.otf'}
for before,after in mapping.items():
    if source.count(before)!=1:raise RuntimeError('Unexpected reference source asset path: '+before)
    source=source.replace(before,str(after).replace('\\','/').replace('"','\\"'))
(src/'desktop-preview.cpp').write_text(source,encoding='utf-8')
shutil.copyfile(ROOT/'reference/source/desktop-preview.pro',src/'desktop-preview.pro')
env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen';env['QT_SCALE_FACTOR']='1';env['QT_AUTO_SCREEN_SCALE_FACTOR']='0'
for key in ('QT_SCREEN_SCALE_FACTORS','QT_FONT_DPI'):env.pop(key,None)
version=subprocess.check_output([a.qmake,'-query','QT_VERSION'],text=True).strip()
if not version.startswith('5.'):raise RuntimeError('Qt5 qmake required')
subprocess.run([a.qmake,str(src/'desktop-preview.pro')],cwd=build,env=env,check=True)
subprocess.run(['make','-j2'],cwd=build,env=env,check=True)
exe=build/'desktop-preview'
subprocess.run([str(exe),'--self-test'],env=env,check=True)
subprocess.run([str(exe),'--render',str(renders)],env=env,check=True)
(output/'render-environment.json').write_text(json.dumps({'qt':version,'platform':'offscreen','scale':1,'source':'approved reference; only asset paths relocated','production_hmi_tested':False},indent=2)+'\n',encoding='utf-8')
print('Reference renders: '+str(renders))
