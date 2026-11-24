"""Native ARM-compatible executor against real PTY Modbus frames (host fixture)."""
import json,os,subprocess,unittest
from pathlib import Path
import test_platform_fieldbus as fixtures
ROOT=Path(__file__).resolve().parents[1]
BINARY=Path(os.environ.get('DSHANCONTROL_FIELDBUS_TEST_BINARY',ROOT/'dist/fieldbus-native/dshancontrol-fieldbus-worker'))

@unittest.skipUnless(BINARY.exists(),'Native fieldbus build required')
class NativeFieldbusTests(fixtures.FieldbusTests):
    def setUp(self):
        super().setUp();manifest=Path(self.temp.name)/'platform.json';manifest.write_text(json.dumps(self.manifest))
        state=Path(self.temp.name)/'state'
        class Executor:
            def execute(inner,action,params):
                p=subprocess.run([str(BINARY),str(manifest),str(state)],input=json.dumps({'action':action,'params':params,'confirmed':True}),text=True,capture_output=True,timeout=15)
                result=json.loads(p.stdout)
                if not result['ok']:raise ValueError(result['code']+': '+result['error'])
                if p.returncode:raise AssertionError(p.stderr)
                return result['result']
        self.bus=Executor()
    def test_ethercat_cli_contract_simulation(self):
        # The native worker only calls the fixed system-owned SOEM CLI. Invalid
        # parameters must be rejected before attempting the external process.
        with self.assertRaisesRegex(ValueError,'Unexpected parameter'):
            self.bus.execute('ethercat_scan',{'program':'/bin/sh'})
        with self.assertRaisesRegex(ValueError,'budget'):
            self.bus.execute('ethercat_cycle',{'count':10000,'period_us':1000000})
    def test_boolean_and_serial_rate_validation(self):
        for action,params in [('can_apply',{'interface':'vcan0','bitrate':True}),
            ('rs485_config',{'port':self.port,'baudrate':12345,'unit_id':1,'poll_ms':1000}),
            ('canopen_sdo_write',{'interface':'vcan0','node_id':1,'index':0x2000,'value':True})]:
            with self.subTest(action=action),self.assertRaises(ValueError):self.bus.execute(action,params)

if __name__=='__main__':unittest.main()
