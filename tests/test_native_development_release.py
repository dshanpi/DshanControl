"""Real Qt TLS, CONNECT proxy and signed pinned Drafts with test-only credentials."""
import copy
import hashlib
import http.server
import json
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import tempfile
import threading
import unittest
import test_application_update as fixtures

ROOT=Path(__file__).resolve().parents[1]
DRIVER=Path(os.environ.get('DSHANCONTROL_DEVELOPMENT_RELEASE_DRIVER',str(ROOT/'dist/native-development-release-tests/development-release-test-driver')))
TOKEN='ghp_TEST_ONLY_'+'a'*32


class NativeDevelopmentReleaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixtures.UpdateTests.setUpClass()
        cls.cert_dir=tempfile.TemporaryDirectory();r=Path(cls.cert_dir.name)
        config=r/'ssl.cnf';config.write_text('[req]\nprompt=no\ndistinguished_name=dn\nx509_extensions=extensions\n[dn]\nCN=api.github.com\n[extensions]\nsubjectAltName=DNS:api.github.com,DNS:release-assets.githubusercontent.com\nbasicConstraints=critical,CA:true\nkeyUsage=critical,digitalSignature,keyEncipherment,keyCertSign\n')
        cls.certificate=r/'certificate.pem';cls.server_key=r/'key.pem'
        subprocess.run(['openssl','req','-new','-x509','-newkey','rsa:2048','-nodes','-days','1','-config',str(config),'-out',str(cls.certificate),'-keyout',str(cls.server_key)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)

    @classmethod
    def tearDownClass(cls):cls.cert_dir.cleanup();fixtures.UpdateTests.tearDownClass()

    def setUp(self):
        self.f=fixtures.UpdateTests();self.addCleanup(self.f.doCleanups);self.f.setUp()
        volatile='/dev/shm' if Path('/dev/shm').is_dir() else '/tmp'
        self.secret=tempfile.TemporaryDirectory(prefix='dshancontrol-development-',dir=volatile);self.addCleanup(self.secret.cleanup)
        self.secret_root=Path(self.secret.name);self.secret_root.chmod(0o700)
        self.token=self.secret_root/'token';self.token.write_text(TOKEN+'\n');self.token.chmod(0o600)
        self.config=self.f.root/'native.json';self.settings=dict(self.f.config,automatic_install=False,development_release={'release_id':73,'token_file':str(self.token)})
        self.save();self.requests=[];self.mode='normal';self.api_assets={}
        for identifier,name,path in [(101,'application-release.json',self.f.path),(102,'application-release.json.sig',self.f.signature),(103,'app.tar.gz',self.f.package)]:
            self.api_assets[identifier]={'id':identifier,'name':name,'state':'uploaded','size':path.stat().st_size,'digest':'sha256:'+hashlib.sha256(path.read_bytes()).hexdigest()}
        self.payloads={101:self.f.path.read_bytes(),102:self.f.signature.read_bytes(),103:self.f.package.read_bytes()}
        self.release={'id':73,'draft':True,'prerelease':False,'tag_name':self.f.manifest['tag'],'assets':list(self.api_assets.values())}
        self.proxy()

    def save(self):self.config.write_text(json.dumps(self.settings));self.config.chmod(0o600)

    def proxy(self):
        test=self
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(str(self.certificate),str(self.server_key))
        class Payload(http.server.BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def reply(self,status,body=b'',location=None):
                self.send_response(status);self.send_header('Content-Length',str(len(body)))
                if location:self.send_header('Location',location)
                self.end_headers();self.wfile.write(body)
            def do_GET(self):
                host=self.headers['Host'];authorization=self.headers.get('Authorization')
                test.requests.append({'host':host,'path':self.path,'authorization_present':authorization is not None,'authorization_matches_fixture':authorization=='Bearer '+TOKEN})
                if host=='api.github.com':
                    if authorization!='Bearer '+TOKEN:self.reply(401);return
                    if self.path=='/repos/dshanpi/DshanControl/releases/73':
                        if test.mode=='metadata-redirect':self.reply(302,location='https://release-assets.githubusercontent.com/metadata');return
                        release=test.release
                        if test.mode=='replace-on-download' and sum(r['path']==self.path for r in test.requests)>1:
                            release=copy.deepcopy(release)
                            release['assets'][2]['id']=104
                        self.reply(200,json.dumps(release).encode());return
                    prefix='/repos/dshanpi/DshanControl/releases/assets/'
                    if not self.path.startswith(prefix):self.reply(404);return
                    identifier=int(self.path[len(prefix):])
                    if identifier not in test.payloads:self.reply(404);return
                    if test.mode=='direct':self.reply(200,test.payloads[identifier]);return
                    target='https://release-assets.githubusercontent.com/asset/'+str(identifier)+'?sig=fixture-only'
                    if test.mode=='evil-redirect':target='https://example.invalid/private'
                    self.reply(302,location=target);return
                if host=='release-assets.githubusercontent.com':
                    if authorization:self.reply(403);return
                    identifier=int(self.path.split('/asset/')[1].split('?')[0]);self.reply(200,test.payloads[identifier]);return
                self.reply(403)
        class Proxy(http.server.BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_CONNECT(self):
                if self.client_address[0] not in getattr(test,'proxy_clients',{'127.0.0.1'}):
                    self.send_error(403);return
                if self.path not in ['api.github.com:443','release-assets.githubusercontent.com:443']:
                    self.send_error(403);return
                self.send_response(200);self.end_headers();self.wfile.flush();self.close_connection=True
                try:
                    with context.wrap_socket(self.connection,server_side=True) as encrypted:Payload(encrypted,self.client_address,self.server)
                except (ssl.SSLError,OSError):pass
        self.server=http.server.ThreadingHTTPServer((getattr(self,'proxy_bind','127.0.0.1'),0),Proxy)
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start();self.addCleanup(self.close_proxy)

    def close_proxy(self):self.server.shutdown();self.server.server_close();self.thread.join(timeout=3)

    def invoke(self,operation='check',candidate=None,expected=0,trust=True):
        spec=self.f.root/'request.json';destination=self.f.root/'download'
        spec.write_text(json.dumps({'configuration':str(self.config),'proxy_port':self.server.server_port,'ca_file':str(self.certificate),
            'trust_fixture_ca':trust,'operation':operation,'candidate':candidate or {},'destination':str(destination)}))
        result=subprocess.run([str(DRIVER),str(spec)],capture_output=True,text=True,timeout=45)
        self.assertEqual(expected,result.returncode,result.stdout+result.stderr)
        self.assertNotIn(TOKEN,result.stdout+result.stderr)
        return json.loads(result.stdout if not expected else result.stderr)

    def test_signed_check_and_redirected_download_keep_credentials_off_cdn(self):
        candidate=self.invoke();self.assertTrue(candidate['development_release'])
        self.assertEqual(73,candidate['release']['id']);self.assertEqual(103,candidate['asset_pins']['package']['id'])
        self.assertNotIn(TOKEN,json.dumps(candidate));self.assertNotIn('sig=fixture-only',json.dumps(candidate))
        target=self.invoke('download',candidate);self.assertEqual('app.tar.gz',target['filename'])
        downloaded=self.f.root/'download/app.tar.gz';self.assertEqual(self.f.package.read_bytes(),downloaded.read_bytes())
        self.assertEqual(self.f.path.read_bytes(),(downloaded.parent/'application-release.json').read_bytes())
        self.assertTrue(any(r['host']=='release-assets.githubusercontent.com' for r in self.requests))
        for request in self.requests:
            self.assertEqual(request['host']=='api.github.com',request['authorization_present'])
            if request['authorization_present']:self.assertTrue(request['authorization_matches_fixture'])
        if os.environ.get('NATIVE_DEVELOPMENT_RELEASE_EVIDENCE'):
            Path(os.environ['NATIVE_DEVELOPMENT_RELEASE_EVIDENCE']).write_text(json.dumps({'signed_candidate_verified':True,'payload_hash_verified':True,
                'cdn_authorization_absent':True,'credential_value_logged':False,'requests':self.requests},indent=2)+'\n')

    def test_direct_asset_stream_is_supported_and_untrusted_tls_is_rejected(self):
        self.mode='direct';self.assertTrue(self.invoke()['development_release'])
        self.assertFalse(any(r['host']!='api.github.com' for r in self.requests))
        self.requests.clear();self.invoke(expected=1,trust=False);self.assertEqual([],self.requests)

    def test_selected_draft_asset_replacement_refuses_before_package_download(self):
        candidate=self.invoke();self.requests.clear()
        self.release['assets'][2]=dict(self.release['assets'][2],id=104)
        error=self.invoke('download',candidate,expected=1)
        self.assertEqual('development-release-changed',error['code'])
        self.assertFalse((self.f.root/'download').exists())
        self.assertFalse(any('/asset/103' in r['path'] or '/assets/103' in r['path'] for r in self.requests))

    def test_unsafe_token_or_non_development_configuration_never_reaches_network(self):
        for change in [{'development_mode':False},{'automatic_install':True},{'automatic_install':'false'},
                       {'development_release':{'release_id':True,'token_file':str(self.token)}}]:
            original=self.settings;self.settings=dict(original,**change);self.save();self.invoke(expected=1);self.settings=original
        self.save();self.token.chmod(0o644);self.assertEqual('credential-permissions',self.invoke(expected=1)['code'])
        self.assertEqual([],self.requests)

    def test_redirects_and_modified_signature_or_payload_fail_without_retargeting(self):
        self.mode='evil-redirect';self.assertEqual('asset-redirect',self.invoke(expected=1)['code'])
        self.mode='metadata-redirect';self.assertEqual('api-redirect',self.invoke(expected=1)['code'])
        self.mode='normal';original=self.payloads[102];self.payloads[102]=b'x'*len(original)
        self.invoke(expected=1);self.payloads[102]=original
        candidate=self.invoke();self.payloads[103]=b'x'*len(self.payloads[103])
        self.invoke('download',candidate,expected=1)
        self.assertFalse((self.f.root/'download/app.tar.gz').exists())
        self.assertTrue((self.f.root/'download/.package.part').exists())
