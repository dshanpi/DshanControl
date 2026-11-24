"""Protocol simulation only; PTYs do not qualify RS485 electrical wiring."""
import importlib.util
import json
import os
from pathlib import Path
import pty
import select
import struct
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('fieldbus',ROOT/'components/platform/fieldbus.py')
bus=importlib.util.module_from_spec(spec);spec.loader.exec_module(bus)

def crc(data):
    value=0xffff
    for byte in data:
        value^=byte
        for _ in range(8): value=(value>>1)^0xa001 if value&1 else value>>1
    return value.to_bytes(2,'little')

class FieldbusTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.master,self.slave=pty.openpty();self.port=os.ttyname(self.slave)
        self.manifest={'capabilities':['modbus-rtu','ethercat','canopen'],'channels':{
            'rs4850':{'kind':'serial','device':self.port,'role':'modbus-rtu'},
            'can0':{'kind':'can','device':'vcan0','role':'canopen'},
            'ethercat0':{'kind':'ethernet','device':'ecat0','role':'ethercat'}}}
        self.bus=bus.Fieldbus(self.manifest,self.temp.name)
    def tearDown(self):
        os.close(self.master);os.close(self.slave);self.temp.cleanup()
    def config(self):
        return self.bus.execute('rs485_config',{'port':self.port,'baudrate':9600,'unit_id':1,'poll_ms':1000})
    def responder(self, frames, corrupt=False):
        errors=[]
        def work():
            try:
                for function,address,value in frames:
                    request=b''
                    while len(request)<8:
                        if not select.select([self.master],[],[],3)[0]: raise AssertionError('No RTU request')
                        request+=os.read(self.master,8-len(request))
                    assert request[-2:]==crc(request[:-2]),request
                    assert request[:6]==struct.pack('>BBHH',1,function,address,value),request
                    response=request[:-2] if function==6 else bytes([1,function,4])+struct.pack('>HH',1234,5678)
                    checksum=b'\0\0' if corrupt else crc(response)
                    os.write(self.master,response+checksum)
            except Exception as error:errors.append(error)
        thread=threading.Thread(target=work,daemon=True);thread.start()
        return thread,errors
    def test_real_pty_modbus_read_write(self):
        self.assertFalse(self.config()['device_acknowledged'])
        thread,errors=self.responder([(3,10,2),(6,20,42)])
        result=self.bus.execute('rs485_read',{'address':10,'count':2})
        self.assertEqual(result['values'],[1234,5678]);self.assertTrue(result['device_acknowledged'])
        self.assertTrue(self.bus.execute('rs485_write',{'address':20,'value':42})['device_acknowledged'])
        thread.join(4);self.assertFalse(thread.is_alive());self.assertEqual(errors,[])
    def test_corrupted_rtu_response_rejected(self):
        self.config();thread,errors=self.responder([(3,10,2)],True)
        with self.assertRaises(Exception) as caught:self.bus.execute('rs485_read',{'address':10,'count':2})
        self.assertIn('checksum',str(caught.exception).lower());thread.join(4);self.assertEqual(errors,[])
    def test_absent_slave_times_out(self):
        self.config()
        with self.assertRaises(Exception):self.bus.execute('rs485_read',{'address':0})
    def test_ports_and_unknown_parameters_rejected_before_io(self):
        with patch.object(bus,'command') as run:
            for action,params in [('can_apply',{'interface':'eth0','bitrate':500000}),
                ('can_apply',{'interface':'vcan0','bitrate':500000,'fd':True,'data_bitrate':2000000}),
                ('ethercat_cycle',{'count':10000,'period_us':1000000}),
                ('ethercat_scan',{'interface':'ecat0','program':'/bin/sh'}),
                ('rs485_config',{'port':'/dev/ttymxc0','baudrate':9600,'unit_id':1,'poll_ms':1000})]:
                with self.subTest(action=action,params=params),self.assertRaises(ValueError):self.bus.execute(action,params)
            run.assert_not_called()
    def test_ethercat_cli_contract_simulation(self):
        # This verifies the SOEM CLI boundary, not a physical EtherCAT slave.
        with patch.object(bus,'command',return_value=json.dumps({'ok':True,'slave':1,'written':2})) as run:
            result=self.bus.execute('ethercat_sdo_write',{'slave':1,'index':'0x2000','subindex':2,'value':'2a 00'})
            self.assertTrue(result['ok'])
            self.assertEqual(run.call_args.args[0],['/usr/bin/omnigate-ethercat','ecat0','sdo-write','1','0x2000','2','2a 00'])
        with patch.object(bus,'command',side_effect=RuntimeError('No EtherCAT slaves')):
            with self.assertRaises(RuntimeError):self.bus.execute('ethercat_scan',{})

if __name__=='__main__':unittest.main()
