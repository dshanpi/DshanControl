"""Qt -> real RuntimeService workers -> real Qt TLS fixture downloads.

Only the test service entry point injects a process-local proxy/CA. Synthetic
keys/tokens, private task/application roots; never production trust or releases.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests'))
from test_native_development_release import NativeDevelopmentReleaseTests, TOKEN


def request(binary, socket_path, method, params=None):
    result=subprocess.run([str(binary),'rpc',method,json.dumps(params or {})],
        env=dict(os.environ,DSHANCONTROL_PLATFORM_SOCKET=str(socket_path)),capture_output=True,text=True,timeout=10)
    if result.returncode:raise AssertionError(result.stderr)
    return json.loads(result.stdout)


def run_case(qt, cli, service, out, mode):
    out.mkdir(parents=True,exist_ok=False)
    fixture=NativeDevelopmentReleaseTests('test_signed_check_and_redirected_download_keep_credentials_off_cdn')
    daemon=None
    try:
        fixture.setUp();fixture.mode=mode
        private=fixture.f.root;apps=fixture.f.app;tasks=private/'tasks';tasks.mkdir()
        hook=private/'system-hook';hook.write_text('#!/bin/sh\nprintf "%s\\n" "$1" >> "'+str(private/'operations')+'"\n[ "$1" != maintenance ] || exit 17\nexit 0\n');hook.chmod(0o700)
        fixture.settings['service_hook']=str(hook);fixture.save()
        socket_path=private/'platform.sock'
        config=private/'service.json';config.write_text(json.dumps({'updater_config':str(fixture.config),
            'task_root':str(tasks),'socket':str(socket_path),'client_gid':os.getgid(),'allowed_uids':[os.getuid()]}))
        spec=private/'service-test.json';spec.write_text(json.dumps({'test_only':True,'fixture_root':str(private),
            'service_configuration':str(config),'proxy_host':'127.0.0.1','proxy_port':fixture.server.server_port,'ca_file':str(fixture.certificate)}))
        before={str(p.relative_to(private)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
            [private/'contract.json',apps/'releases/factory/installed.json',fixture.f.data]}
        with (out/'daemon.log').open('xb') as log:daemon=subprocess.Popen([str(service),str(spec)],stdout=log,stderr=subprocess.STDOUT)
        for _ in range(200):
            if daemon.poll() is not None:raise AssertionError((out/'daemon.log').read_text())
            if socket_path.exists():break
            time.sleep(.02)
        assert socket_path.exists()
        began=time.monotonic()
        env=dict(os.environ,QT_QPA_PLATFORM='offscreen',DSHANCONTROL_NATIVE_PLATFORM_SOCKET=str(socket_path),
            DSHANCONTROL_UPDATER_EXECUTABLE=str(cli),DSHANCONTROL_TEST_UPDATE_FLOW=mode,
            DSHANCONTROL_TEST_UI_EVIDENCE=str(out/'ui-result.json'))
        result=subprocess.run([str(qt),'-o',str(out/'qt-results.xml')+',xml'],env=env,capture_output=True,text=True,timeout=300)
        (out/'qt-test.log').write_text(result.stdout+result.stderr)
        assert result.returncode==0,(result.stdout,result.stderr,(out/'qt-results.xml').read_text())
        incidents=[n.get('type') for n in ET.parse(out/'qt-results.xml').findall('.//Incident')];assert incidents==['pass']*3
        ui=json.loads((out/'ui-result.json').read_text());checked=request(cli,socket_path,'update.status',{'task_id':ui['check_task_id']})
        assert checked['operation']=='check' and checked['status']=='success' and checked['candidate']['development_release'] is True
        failed=request(cli,socket_path,'update.status',{'task_id':ui['failed_task_id']});assert failed['status']=='failed'
        records=[json.loads(p.read_text()) for p in tasks.glob('*.json') if p.name!='latest.json']
        operations=(private/'operations').read_text().splitlines() if (private/'operations').exists() else []
        downloaded=None
        if mode=='normal':
            downloaded=request(cli,socket_path,'update.status',{'task_id':ui['download_task_id']})
            assert downloaded['operation']=='download' and downloaded['status']=='success'
            assert downloaded['source_task_id']==ui['check_task_id'] and failed['source_task_id']==ui['download_task_id']
            assert failed['operation']=='install' and failed['error']['stage']=='maintenance' and failed['error']['code']=='17'
            assert operations==['maintenance'] and len(records)==3
            assert (tasks/ui['download_task_id']/'app.tar.gz').read_bytes()==fixture.f.package.read_bytes()
        else:
            assert failed['operation']=='download' and failed['source_task_id']==ui['check_task_id']
            assert failed['error']['code']=='development-release-changed' and operations==[] and len(records)==2
            assert not any('/asset/103' in r['path'] or '/assets/103' in r['path'] or '/asset/104' in r['path'] or '/assets/104' in r['path'] for r in fixture.requests)
        for r in fixture.requests:
            assert r['authorization_present']==(r['host']=='api.github.com')
            if r['authorization_present']:assert r['authorization_matches_fixture']
        assert any(r['host']=='release-assets.githubusercontent.com' for r in fixture.requests)
        assert fixture.f.updater.current_path().name=='factory' and fixture.f.updater.state()['high_water']==0
        assert not (apps/'transaction.json').exists()
        for name,sha in before.items():assert hashlib.sha256((private/name).read_bytes()).hexdigest()==sha
        report={'passed':True,'scope':'Private synthetic TLS release, actual native workers and Qt; test-only proxy/CA entry point; not public GitHub/production install',
            'mode':mode,'elapsed_seconds':time.monotonic()-began,'qt_incidents':incidents,'ui':ui,'checked':checked,
            'downloaded':downloaded,'failed':failed,'operations':operations,'task_count':len(records),'requests':fixture.requests,
            'cdn_authorization_absent':True,'preserved_hashes':before,'current':'factory','high_water':0,
            'sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [qt,cli,service]}}
        text=json.dumps(report,ensure_ascii=False,indent=2);assert TOKEN not in text
        (out/'result.json').write_text(text+'\n');print(json.dumps({'mode':mode,'passed':True,'seconds':report['elapsed_seconds']}))
    finally:
        if daemon is not None and daemon.poll() is None:
            try:request(cli,socket_path,'service.shutdown');daemon.wait(timeout=5)
            except Exception:daemon.terminate();daemon.wait(timeout=5)
        fixture.doCleanups()


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['qt','cli','service','output']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();NativeDevelopmentReleaseTests.setUpClass()
    try:
        for mode in ['normal','replace-on-download']:run_case(a.qt.resolve(),a.cli.resolve(),a.service.resolve(),a.output.resolve()/mode,mode)
    finally:NativeDevelopmentReleaseTests.tearDownClass()
