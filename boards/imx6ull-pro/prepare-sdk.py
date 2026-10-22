from pathlib import Path
import argparse, gzip, hashlib, json, shutil, subprocess
p=argparse.ArgumentParser();p.add_argument('sdk',type=Path);a=p.parse_args();sdk=a.sdk.resolve()
board=Path(__file__).resolve().parent
expected=json.loads((board/'source-identity.json').read_text())
sdk_rev=subprocess.check_output(['git','-C',str(sdk),'rev-parse','HEAD'],text=True).strip()
if sdk_rev!=expected['sdk_commit']:raise SystemExit('Unexpected SDK repository revision')
from source_inputs import application_identity
identity=application_identity(board.parents[1],expected)
sources=sdk/'sources';sources.mkdir(exist_ok=True)
receipt={}
(sources/'dshancontrol-build-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
for name,key in [('linux-imx','kernel_commit'),('uboot-imx','uboot_commit')]:
    rev=subprocess.check_output(['git','-C',str(sdk/name),'rev-parse','HEAD'],text=True).strip()
    if rev!=expected[key]:raise SystemExit('Unexpected SDK submodule revision: '+name)
    dirty=subprocess.check_output(['git','-C',str(sdk/name),'status','--porcelain'],text=True)
    if dirty:raise SystemExit('Refusing unrecorded submodule changes: '+name)
    dest=sources/(name+'-'+rev+'.tar.gz')
    if not dest.exists():
        temp=dest.with_suffix('.part')
        with gzip.open(temp,'wb',compresslevel=1) as out:
            proc=subprocess.Popen(['git','-C',str(sdk/name),'archive','--format=tar','--prefix='+name+'/',rev],stdout=subprocess.PIPE)
            shutil.copyfileobj(proc.stdout,out)
            if proc.wait()!=0:raise SystemExit('git archive failed')
        temp.replace(dest)
    digest=hashlib.sha256()
    with dest.open('rb') as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b''):digest.update(chunk)
    receipt[name]={'commit':rev,'archive':str(dest),'sha256':digest.hexdigest()}
config=(board/'defconfig.in').read_text().replace('@BOARD@',str(board)).replace('@SOURCES@',str(sources))
(sdk/'buildroot-2026.02.1/configs/100ask_imx6ull_dshancontrol_defconfig').write_text(config)
(sources/'dshancontrol-sdk-inputs.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('SDK configuration and pinned source archives prepared')
