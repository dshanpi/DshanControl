import argparse, json, shutil, os, time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--target',type=Path,required=True);p.add_argument('--identity',type=Path,required=True);a=p.parse_args()
s=a.source;t=a.target.resolve();b=s/'boards/imx6ull-pro'
assert str(t)!='/'
def cp(src,dst,mode=0o644):
    dst=t/dst;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dst);dst.chmod(mode)
def tree(src,dst):
    shutil.copytree(src,t/dst,dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
for f in ('system-contract.json','updater.json','platform-service.json','desktop.json'):
    cp(b/f,'etc/dshancontrol/'+('system-release.json' if f=='system-contract.json' else f),0o600)
cp(b/'platform.json','etc/omnigate/platform.json')
for f in ('S80dshancontrol','S50adbd'):cp(b/'runtime'/f,'etc/init.d/'+f,0o755)
cp(b/'runtime/display.env','etc/dshancontrol/display.env')
identity=json.loads(a.identity.read_text())
reference=json.loads((b/'source-identity.json').read_text())
assert identity['application_base_commit']==reference['application_commit']
for field in ('sdk_commit','kernel_commit','uboot_commit','application_patch_sha256'):
    assert identity[field]==reference[field], 'Build source identity mismatch: '+field
assert type(identity['application_dirty']) is bool
commit=identity['application_commit']
(t/'etc/dshancontrol/application-commit').write_text(commit+'\n')
factory='data/dshancontrol/apps/releases/factory'
for name,path in [('omnigate-hmi','components/hmi/src'),('dshancontrol-desktop-service','components/desktop'),('dshancontrol-media-worker','components/media')]:
    cp(s/path/name,factory+'/bin/'+name,0o755)
cp(s/'components/runtime/dshancontrol-runtime','usr/bin/dshancontrol-runtime',0o755)
tree(s/'components/core/src/omnigate_core',factory+'/lib/core/omnigate_core')
for f in ('app.py','hmi_api.py'):cp(s/'components/web'/f,factory+'/lib/web/'+f)
tree(s/'components/web/assets',factory+'/share/web')
cp(s/'components/hmi/src/fonts/OFL.txt',factory+'/share/licenses/DshanSans-OFL.txt')
cp(s/'components/hmi/src/fonts/manifest.json',factory+'/share/licenses/DshanSans-manifest.json')
cp(s/'LICENSE',factory+'/share/licenses/DshanControl-GPL3.txt')
cp(s/'components/hmi/third_party/pinyin/NOTICE',factory+'/share/licenses/pinyin-NOTICE')
installed={'version':'0.1.0-dev-imx6ull-preview','commit':commit,'sequence':0,'data_schema':1}
(t/factory/'installed.json').write_text(json.dumps(installed,indent=2)+'\n')
current=t/'data/dshancontrol/apps/current'
if not current.is_symlink():current.symlink_to('releases/factory')
data=t/'data/dshancontrol/data';data.mkdir(parents=True,exist_ok=True);data.chmod(0o700)
web=json.loads((s/'components/web/config/config.json').read_text());web['can']={};web['canopen']={'nodes':{}};web['ec20']={'profiles':{}}
for name,value in [('web.json',web),('hmi.json',json.loads((s/'components/hmi/hmi-config.json').read_text()))]:
    dest=data/name
    if not dest.exists():dest.write_text(json.dumps(value,indent=2)+'\n');dest.chmod(0o600)
provider=t/'data/dshancontrol/provider';provider.mkdir(parents=True,exist_ok=True);provider.chmod(0o700)
(t/'data/dshancontrol/desktop').mkdir(parents=True,exist_ok=True)
(t/'data/media').mkdir(parents=True,exist_ok=True)
fstab=t/'etc/fstab';line='/dev/mmcblk1p3 /data ext4 defaults,noatime,nosuid,nodev 0 2\n'
if line not in fstab.read_text():fstab.write_text(fstab.read_text()+line)
cp(a.identity,'etc/dshancontrol/source-identity.json')

# A development board without an RTC backup must still have a sane clock before Web sessions.
cp(b/'runtime/dshancontrol-clock','usr/sbin/dshancontrol-clock',0o755)
cp(b/'runtime/S05dshanclock','etc/init.d/S05dshanclock',0o755)
(t/'etc/dshancontrol/build-clock-utc').write_text(str(int(os.environ.get('SOURCE_DATE_EPOCH') or time.time()))+'\n')
cron=t/'etc/cron/crontabs/root';cron.parent.mkdir(parents=True,exist_ok=True)
line='*/5 * * * * /usr/sbin/dshancontrol-clock save\n'
old=cron.read_text() if cron.exists() else ''
if line not in old:cron.write_text(old+line)
cron.chmod(0o600)

cp(b/'runtime/S41wifi','etc/init.d/S41wifi',0o755)
network=t/'data/dshancontrol/network';network.mkdir(parents=True,exist_ok=True);network.chmod(0o700)
