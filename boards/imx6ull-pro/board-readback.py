"""Read-only checks on the bound i.MX6ULL development board. No flash writes."""
import argparse,hashlib,json,os,socket,stat,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('expected',type=Path);a=p.parse_args();expected=json.loads(a.expected.read_text())
checks=[]
def check(name,actual,wanted):
    if actual!=wanted:raise RuntimeError(f'{name}: actual {actual!r}, expected {wanted!r}')
    checks.append({'name':name,'passed':True,'actual':actual})
def digest(path,offset=0,count=None):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        f.seek(offset)
        while count is None or count:
            b=f.read(1024*1024 if count is None else min(count,1024*1024))
            if not b:break
            h.update(b)
            if count is not None:count-=len(b)
    if count not in (None,0):raise RuntimeError('Truncated block read')
    return h.hexdigest()
identity=json.loads(Path('/etc/dshancontrol/source-identity.json').read_text());check('source_identity',identity,expected['identity'])
check('kernel_release',os.uname().release,'5.4.47')
check('emmc_bytes',int(Path('/sys/class/block/mmcblk1/size').read_text())*512,expected['board_capacity'])
for part in expected['partitions']:
    check('partition_start_'+str(part['number']),int(Path('/sys/class/block/mmcblk1p'+str(part['number'])+'/start').read_text())*512,part['offset'])
check('bootloader_readback_sha256',digest('/dev/mmcblk1',1024,expected['ivt']['boot_bytes']-1024),expected['sha256']['u-boot-dtb.imx'])
boot=expected['partitions'][0];check('boot_fat_readback_sha256',digest('/dev/mmcblk1',boot['offset'],boot['bytes']),expected['sha256']['boot.vfat'])
for name,sha in expected['binary_sha256'].items():
    path='/usr/bin/'+name if name=='dshancontrol-runtime' else '/data/dshancontrol/apps/current/bin/'+name
    check(name+'_sha256',digest(path),sha)
for path,mode in [('/data/dshancontrol/data',0o700),('/data/dshancontrol/provider',0o700),('/data/dshancontrol/data/web.json',0o600)]:
    st=os.stat(path);check(path+'_ownership',[st.st_uid,st.st_gid,stat.S_IMODE(st.st_mode)],[0,0,mode])
check('lcd_size',Path('/sys/class/graphics/fb0/virtual_size').read_text().strip(),'1024,600')
check('lcd_bpp',int(Path('/sys/class/graphics/fb0/bits_per_pixel').read_text()),32)
check('touch_driver',Path('/sys/class/input/event0/device/name').read_text().strip(),'goodix-ts')
with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as s:
    s.settimeout(5);s.connect('/run/dshancontrol/desktop/desktop.sock');s.sendall(b'{"protocol":1,"method":"desktop.snapshot","params":{}}\n');data=b''
    while not data.endswith(b'\n'):
        chunk=s.recv(4096)
        if not chunk or len(data)>65536:raise RuntimeError('Invalid desktop RPC response')
        data+=chunk
    value=json.loads(data);check('native_desktop_rpc',value['ok'],True);check('native_desktop_sections',sorted(value['result']),['cloud','home','reporting','version','voice'])
check('sane_wall_clock',int(time.time())>=int(Path('/etc/dshancontrol/build-clock-utc').read_text()),True)
check('clock_checkpoint_permissions',stat.S_IMODE(os.stat('/etc/cron/crontabs/root').st_mode),0o600)
ntpd=int(Path('/var/run/ntpd.pid').read_text());os.kill(ntpd,0);check('ntpd_process_running',True,True)
first=json.loads(Path('/run/dshancontrol/hmi-heartbeat.json').read_text());time.sleep(2);last=json.loads(Path('/run/dshancontrol/hmi-heartbeat.json').read_text())
check('qt_event_loop_advances',last['monotonic_seconds']>first['monotonic_seconds'],True);check('backend_identity_verified',last['backend_identity_verified'],True)
print(json.dumps({'status':'passed','scope':'readback of bootloader, boot FAT and installed application files; not full mutated root/data filesystem comparison','checks':checks,'heartbeat':last},indent=2))
