"""Read-only development image checks before flashing the bound i.MX6ULL Pro."""
import argparse, hashlib, json, struct, subprocess, tempfile
from pathlib import Path

CAPACITY=3959422976
def require(value,message):
    if not value:raise ValueError(message)
def digest(path,offset=0,size=None):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        f.seek(offset)
        while size is None or size:
            chunk=f.read(1024*1024 if size is None else min(size,1024*1024))
            if not chunk:break
            h.update(chunk)
            if size is not None:size-=len(chunk)
    require(size in (None,0),'Truncated image slice')
    return h.hexdigest()
def ivt(blob):
    require(len(blob)>=48,'Truncated i.MX boot header')
    require(blob[:4]==bytes.fromhex('d1002040'),'Unexpected i.MX6 IVT header')
    entry,reserved,dcd,boot,selfptr,csf,reserved2=struct.unpack_from('<7I',blob,4)
    require(reserved==reserved2==csf==0,'Unexpected reserved/CSF fields in unsigned development loader')
    require(0x80000000<=selfptr<entry<0xa0000000,'Loader outside this board DDR range')
    boot_offset=boot-selfptr;dcd_offset=dcd-selfptr
    require(boot_offset==32 and dcd_offset==44,'Unexpected i.MX6 boot/DCD layout')
    start,size,plugin=struct.unpack_from('<3I',blob,boot_offset)
    require(plugin==0 and selfptr-start==1024 and size==len(blob)+1024,'Loader boot-data length/start mismatch')
    require(blob[dcd_offset]==0xd2 and blob[dcd_offset+3]==0x40,'Invalid DCD header')
    dcd_length=struct.unpack_from('>H',blob,dcd_offset+1)[0]
    require(dcd_length>=4 and dcd_offset+dcd_length<=entry-selfptr<len(blob),'DCD crosses loader entry/payload')
    return {'entry':hex(entry),'self':hex(selfptr),'boot_start':hex(start),'boot_bytes':size,'dcd_bytes':dcd_length}
def partitions(blob,total):
    require(len(blob)>=512 and blob[510:512]==b'\x55\xaa','Invalid disk MBR signature')
    out=[];end=0
    for n in range(4):
        row=blob[446+n*16:462+n*16]
        active,kind=row[0],row[4];start,count=struct.unpack_from('<II',row,8)
        if not kind:
            require(start==count==0,'Nonempty unused partition');continue
        require(n<3 and active in (0,128) and kind==(12 if n==0 else 131),'Unexpected partition type/boot flag')
        offset=start*512;size=count*512
        require(size>0 and offset>=end and offset+size<=total,'Overlapping/out-of-bounds partition')
        if n==0:require(active==128 and offset==8*1024*1024,'Wrong boot partition offset/flag')
        out.append({'number':n+1,'type':kind,'offset':offset,'bytes':size});end=offset+size
    require(len(out)==3,'Expected boot/rootfs/data partitions')
    return out
def arm(path):
    with path.open('rb') as f:b=f.read(52)
    require(len(b)==52 and b[:7]==b'\x7fELF\x01\x01\x01','Expected ELF32 little endian target binary: '+str(path))
    require(struct.unpack_from('<H',b,18)[0]==40,'Non-ARM binary: '+str(path))
    flags=struct.unpack_from('<I',b,36)[0]
    require(flags&0x400 and not flags&0x200,'Non-hard-float ARM ABI: '+str(path))
    return digest(path)
def uuu(script):
    rows=[row.strip() for row in script.splitlines() if row.strip() and not row.lstrip().startswith('#')]
    expected=['uuu_version 1.4.193','SDP: boot -f u-boot-dtb.imx','FB: ucmd setenv fastboot_dev mmc','FB: ucmd mmc dev 1','FB: flash -raw2sparse all sdcard.img','FB: flash bootloader u-boot-dtb.imx','FB: done']
    require(rows==expected,'Unexpected board UUU sequence; reset must occur separately after task success')
def main():
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--capacity',type=int,default=CAPACITY);a=p.parse_args()
    output=a.output.resolve();images=output/'images';disk=images/'sdcard.img'
    require(disk.stat().st_size<=a.capacity,'Image exceeds measured board capacity')
    loader=images/'u-boot-dtb.imx';header=ivt(loader.read_bytes())
    kernel=(images/'zImage').read_bytes()
    require(len(kernel)>48 and struct.unpack_from('<I',kernel,36)[0]==0x016f2818,'Not an ARM zImage')
    dtb=(images/'imx6ull-14x14-evk.dtb').read_bytes()
    require(dtb[:4]==bytes.fromhex('d00dfeed') and b'fsl,imx6ull\x00' in dtb,'DTB does not identify i.MX6ULL')
    with disk.open('rb') as f:parts=partitions(f.read(512),disk.stat().st_size)
    require(parts[0]['bytes']==32*1024*1024,'Unexpected boot filesystem size')
    require(digest(disk,parts[0]['offset'],parts[0]['bytes'])==digest(images/'boot.vfat'),'Packed boot filesystem differs')
    with tempfile.TemporaryDirectory(prefix='imx6ull-fat-check-',dir=output/'build') as temporary:
        for name in ['zImage','imx6ull-14x14-evk.dtb']:
            copied=Path(temporary)/name
            subprocess.run([str(output/'host/bin/mcopy'),'-i',str(images/'boot.vfat'),'::'+name,str(copied)],check=True,capture_output=True)
            require(digest(copied)==digest(images/name),'Boot FAT contains stale/mismatched '+name)
    require(digest(disk,1024,loader.stat().st_size)==digest(loader),'Packed bootloader differs from selected artifact')
    for n,name in [(1,'rootfs.ext4'),(2,'data.ext4')]:
        image=images/name;part=parts[n]
        require(part['bytes']==image.stat().st_size,'Filesystem and partition size mismatch: '+name)
        require(digest(disk,part['offset'],part['bytes'])==digest(image),'Packed filesystem differs: '+name)
    uuu((images/'flash-sdp.uuu').read_text())
    kernel_config=(output/'build/linux-custom/.config').read_text()
    for line in ['CONFIG_CFG80211=m','CONFIG_MAC80211=m','CONFIG_RTL8XXXU=m','CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y','# CONFIG_FB_MXC_OVERLAY is not set']:
        require(line in kernel_config,'Unexpected Lite kernel setting: '+line)
    require((output/'build/linux-custom/net/wireless/certs/wens.hex').is_file(),'Missing current regulatory signer public certificate')
    target=output/'target'
    for name in ['lib/firmware/rtlwifi/rtl8723bu_nic.bin','lib/firmware/rtl_bt/rtl8723b_fw.bin','lib/firmware/regulatory.db','lib/firmware/regulatory.db.p7s','etc/init.d/S21rngd','etc/init.d/S05dshanclock','usr/sbin/dshancontrol-clock','usr/sbin/ntpd','usr/bin/ntpq','usr/sbin/wpa_supplicant','usr/sbin/wpa_cli','usr/sbin/iw','etc/init.d/S41wifi','usr/lib/gstreamer-1.0/libgstlibav.so','usr/lib/gstreamer-1.0/libgstapp.so','usr/lib/gstreamer-1.0/libgstplayback.so','usr/libexec/gstreamer-1.0/gst-plugin-scanner']:
        require((target/name).is_file(),'Missing startup dependency: '+name)
    require(not (target/'data/dshancontrol/network/wpa_supplicant.conf').exists(),'Device Wi-Fi credentials must not enter the image')
    require(int((target/'etc/dshancontrol/build-clock-utc').read_text())>=1577836800,'Invalid build clock floor')
    require('/usr/sbin/dshancontrol-clock save' in (target/'etc/cron/crontabs/root').read_text(),'Missing periodic clock checkpoint')
    interfaces=(target/'etc/network/interfaces').read_text()
    require('auto eth0' in interfaces and 'iface eth0 inet dhcp' in interfaces,'Missing eth0 DHCP configuration')
    board=target/'etc/dshancontrol/source-identity.json'
    identity=json.loads(board.read_text())
    base=identity.get('application_base_commit',identity['application_commit'])
    require(base=='0584604f5c038fb21040a1cd6c90ab2f5b5e847a','Wrong preview source base')
    require(len(identity['application_commit'])==40 and all(c in '0123456789abcdef' for c in identity['application_commit']),'Invalid application commit')
    if 'application_dirty' in identity:
        require(type(identity['application_dirty']) is bool,'Build dirtiness must be explicit')
    require(identity['kernel_commit']=='05967a3aaba135f40eee6eaf14ced11c090d32c1','Wrong SDK kernel source')
    bins={}
    base=target/'data/dshancontrol/apps/releases/factory/bin'
    for name in ['omnigate-hmi','dshancontrol-desktop-service','dshancontrol-media-worker']:bins[name]=arm(base/name)
    bins['dshancontrol-runtime']=arm(target/'usr/bin/dshancontrol-runtime')
    debugfs=output/'host/sbin/debugfs'
    for path in ['/dshancontrol/provider','/dshancontrol/data','/dshancontrol/data/web.json']:
        result=subprocess.run([str(debugfs),'-R','stat '+path,str(images/'data.ext4')],text=True,capture_output=True,check=True).stdout
        require('User:     0   Group:     0' in result,'Data image has non-root ownership: '+path)
        mode='0600' if path.endswith('.json') else '0700'
        require('Mode:  '+mode in result,'Data image has unsafe private permissions: '+path)
    report={'status':'passed','scope':'static source/ABI/packing checks; not board or signature verification','identity':identity,'ivt':header,'partitions':parts,'board_capacity':a.capacity,'image_bytes':disk.stat().st_size,'binary_sha256':bins,'sha256':{name:digest(images/name) for name in ['u-boot-dtb.imx','zImage','imx6ull-14x14-evk.dtb','rootfs.ext4','data.ext4','sdcard.img','flash-sdp.uuu','boot.vfat']}}
    (images/'imx6ull-static-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
