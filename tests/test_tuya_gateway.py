import hashlib,hmac,importlib.util,json,os,shutil,subprocess,tempfile,threading,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('gateway',ROOT/'components/integrations/tuya_gateway.py')
gateway=importlib.util.module_from_spec(spec);spec.loader.exec_module(gateway)

class FakeTuya:
    def __init__(self):self.posts=[];self.online=True;self.drop=False;self.reported=False;self.kind='Boolean'
    def call(self,method,path,body=None):
        if method=='POST':
            self.posts.append(body)
            if self.drop:raise TimeoutError('outcome unknown')
            return True
        if path.endswith('/functions'):return {'functions':[{'code':'switch','type':self.kind,'values':'{}'}]}
        return {'id':'lamp','name':'fixture','online':self.online,'status':[{'code':'switch','value':self.reported}]}

class TuyaGatewayTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.token='test-restricted-session-0000000000000000';p=self.root/'token';p.write_text(self.token);p.chmod(0o600)
        self.api=FakeTuya();self.config={'desktop_token_file':str(p),'command_database':str(self.root/'commands.db'),
            'devices':{'lamp':['switch']}}
        self.client=gateway.create_app(self.config,self.api).test_client()
        self.headers={'Authorization':'Bearer '+self.token,'Idempotency-Key':'0123456789abcdef0123456789abcdef'}
        self.body={'code':'switch','value':True}
    def tearDown(self):self.temp.cleanup()
    def test_signing_canonical_body_token_and_path(self):
        body=b'{"x":1}';path='/v1.0/devices/lamp/commands'
        canonical='POST\n'+hashlib.sha256(body).hexdigest()+'\n\n'+path
        expected=hmac.new(b'secret',('client'+'token'+'1700000000000'+canonical).encode(),hashlib.sha256).hexdigest().upper()
        self.assertEqual(gateway.signature('client','secret','token','1700000000000','POST',path,body),expected)
        self.assertNotEqual(gateway.signature('client','secret','','1700000000000','POST',path,body),expected)
    def test_list_functions_and_single_command(self):
        self.assertEqual(self.client.get('/v1/devices',headers=self.headers).json['devices'][0]['id'],'lamp')
        self.assertTrue(self.client.get('/v1/devices/lamp/functions',headers=self.headers).json['control_allowed'])
        reply=self.client.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body)
        self.assertEqual(reply.status_code,200);self.assertEqual(reply.json,{'accepted':True,'state_confirmed':False})
        self.assertEqual(self.client.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body).status_code,409)
        self.assertEqual(len(self.api.posts),1)
        self.assertEqual(self.api.posts[0],{'commands':[self.body]})
    def test_desktop_capability_case_and_upstream_envelope(self):
        self.api.kind='boolean'
        response=self.client.get('/v1/devices/lamp/functions',headers=self.headers)
        self.assertEqual(response.json['functions'][0]['type'],'Boolean')
        self.assertEqual(response.json['functions'][0]['values'],{})
        response=self.client.post('/v1/devices/lamp/commands',headers=self.headers,json={'commands':[self.body]})
        self.assertEqual(response.status_code,400);self.assertFalse(self.api.posts)
    def test_idempotency_survives_gateway_restart(self):
        self.assertEqual(self.client.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body).status_code,200)
        restarted=gateway.create_app(self.config,self.api).test_client()
        self.assertEqual(restarted.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body).status_code,409)
        self.assertEqual(len(self.api.posts),1)
    def test_timeout_is_not_retried(self):
        self.api.drop=True
        self.assertEqual(self.client.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body).status_code,502)
        self.assertEqual(self.client.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body).status_code,409)
        self.assertEqual(len(self.api.posts),1)
    def test_unauthorized_offline_and_wrong_value(self):
        self.assertEqual(self.client.get('/v1/devices').status_code,403)
        self.assertEqual(self.client.post('/v1/devices/unlisted/commands',headers=self.headers,json=self.body).status_code,403)
        self.api.online=False
        self.assertEqual(self.client.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body).status_code,409)
        self.api.online=True;self.headers['Idempotency-Key']='new-key-00000000000000000000000000';self.body['value']='true'
        self.assertEqual(self.client.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body).status_code,400)
        self.assertFalse(self.api.posts)

    def test_real_desktop_tls_gateway_command_and_reported_state(self):
        from test_desktop_service import BINARY,eventually,rpc
        from werkzeug.serving import make_server,WSGIRequestHandler
        if not BINARY.exists() or not shutil.which('openssl'):self.skipTest('Native desktop and openssl required')
        cert=self.root/'cert.pem';key=self.root/'fixture.key'
        subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','1','-keyout',str(key),'-out',str(cert),
            '-subj','/CN=localhost','-addext','subjectAltName=DNS:localhost,IP:127.0.0.1'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=15)
        class Quiet(WSGIRequestHandler):
            def log(self,*args,**kwargs):pass
        server=make_server('127.0.0.1',0,gateway.create_app(self.config,self.api),threaded=True,ssl_context=(str(cert),str(key)),request_handler=Quiet)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        config=self.root/'desktop.json';sock=self.root/'ipc/desktop.sock'
        config.write_text(json.dumps({'https_ca_file':str(cert),'home':{'endpoint':'https://localhost:'+str(server.server_port),'token_file':str(self.root/'token')}}));config.chmod(0o600)
        process=None
        try:
            process=subprocess.Popen([str(BINARY),'--socket',str(sock),'--config',str(config)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            eventually(sock.exists)
            state=lambda:rpc(sock)['result']['home']
            self.assertTrue(rpc(sock,'home.refresh')['ok']);eventually(lambda:state()['fresh'] and not state()['busy'])
            self.assertTrue(rpc(sock,'home.functions',{'device_id':'lamp'})['ok']);eventually(lambda:state()['functions'].get('lamp') and not state()['busy'])
            result=rpc(sock,'home.command',{'device_id':'lamp','code':'switch','value':True,'command_id':'b'*32,'revision':state()['revision'],'confirmed':True})
            self.assertTrue(result['ok'],result)
            eventually(lambda:len(self.api.posts)==1 and not state()['busy'])
            self.assertEqual(self.api.posts,[{'commands':[{'code':'switch','value':True}]}])
            self.assertEqual(state()['pending']['lamp/switch']['state'],'awaiting_confirmation')
            self.assertFalse(state()['devices'][0]['status'][0]['value'])
            self.api.reported=True
            self.assertTrue(rpc(sock,'home.refresh')['ok']);eventually(lambda:not state()['pending'])
            self.assertTrue(state()['devices'][0]['status'][0]['value'])
            self.assertEqual(len(self.api.posts),1)
        finally:
            if process is not None:
                process.terminate()
                try:process.wait(5)
                except subprocess.TimeoutExpired:process.kill();process.wait(5)
            server.shutdown();server.server_close();thread.join(3)

if __name__=='__main__':unittest.main()
