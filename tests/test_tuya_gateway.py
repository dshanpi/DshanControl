import hashlib,hmac,importlib.util,json,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('gateway',ROOT/'components/integrations/tuya_gateway.py')
gateway=importlib.util.module_from_spec(spec);spec.loader.exec_module(gateway)

class FakeTuya:
    def __init__(self):self.posts=[];self.online=True;self.drop=False
    def call(self,method,path,body=None):
        if method=='POST':
            self.posts.append(body)
            if self.drop:raise TimeoutError('outcome unknown')
            return True
        if path.endswith('/functions'):return {'functions':[{'code':'switch','type':'Boolean','values':'{}'}]}
        return {'id':'lamp','name':'fixture','online':self.online,'status':[{'code':'switch','value':False}]}

class TuyaGatewayTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.token='test-restricted-session-0000000000000000';p=self.root/'token';p.write_text(self.token);p.chmod(0o600)
        self.api=FakeTuya();self.client=gateway.create_app({'desktop_token_file':str(p),'command_database':str(self.root/'commands.db'),
            'devices':{'lamp':['switch']}},self.api).test_client()
        self.headers={'Authorization':'Bearer '+self.token,'Idempotency-Key':'0123456789abcdef0123456789abcdef'}
        self.body={'commands':[{'code':'switch','value':True}]}
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
        self.api.online=True;self.headers['Idempotency-Key']='new-key-00000000000000000000000000';self.body['commands'][0]['value']='true'
        self.assertEqual(self.client.post('/v1/devices/lamp/commands',headers=self.headers,json=self.body).status_code,400)
        self.assertFalse(self.api.posts)

if __name__=='__main__':unittest.main()
