"""SocketCAN protocol tests with a software CANopen slave on an isolated vcan bus."""
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'dist/fieldbus-native/dshancontrol-fieldbus-worker'
INTERFACE = os.environ.get('DSHANCONTROL_TEST_CAN', '')
FRAME = struct.Struct('=IB3x8s')


@unittest.skipUnless(BINARY.exists() and INTERFACE, 'Set isolated DSHANCONTROL_TEST_CAN for SocketCAN tests')
class NativeCanopenTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.manifest = Path(self.temp.name) / 'manifest.json'
        self.manifest.write_text(json.dumps({'capabilities': ['canopen'], 'channels': {'can': {'kind': 'can', 'device': INTERFACE}}}))
        self.stop = threading.Event(); self.error = []; self.received = []
        self.data = b'Native segmented object dictionary payload 1234567890'
        self.bad_toggle = False; self.wrong_object = False; self.abort = False
        self.objects = {}
        self.sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        self.sock.bind((INTERFACE,)); self.sock.settimeout(.05)
        self.worker = threading.Thread(target=self.slave); self.worker.start()
        self.addCleanup(self.close)

    def close(self):
        self.stop.set(); self.worker.join(timeout=2); self.sock.close()
        self.assertFalse(self.worker.is_alive()); self.assertEqual(self.error, [])

    def send(self, payload):
        self.sock.send(FRAME.pack(0x581, 8, payload.ljust(8, b'\0')))

    def slave(self):
        upload_offset = 0; download = None
        try:
            while not self.stop.is_set():
                try: raw = self.sock.recv(FRAME.size)
                except socket.timeout: continue
                ident, size, data = FRAME.unpack(raw)
                if ident == 0: self.received.append(('nmt', data[:size])); continue
                if ident == 0x201: self.received.append(('pdo',data[:size])); continue
                if ident != 0x601: continue
                c = data[0]
                if download is not None and c & 0xe0 == 0:
                    download += data[1:8 - ((c >> 1) & 7)]
                    self.send(bytes([0x20 | (c & 0x10)]))
                    if c & 1: self.received.append(('write', bytes(download))); download = None
                elif c == 0x40:
                    index,sub=struct.unpack('<HB',data[1:4])
                    if self.objects:
                        value=self.objects.get((index,sub))
                        if value is None:self.send(b'\x80'+data[1:4]+bytes.fromhex('00000206'));continue
                        self.data=value
                    if self.abort:
                        self.send(b'\x80' + data[1:4] + bytes.fromhex('00000206')); continue
                    object_id = data[1:4] if not self.wrong_object else b'\xff\xff\xff'
                    upload_offset = 0
                    if len(self.data) <= 4:
                        self.send(bytes([0x43 | ((4 - len(self.data)) << 2)]) + object_id + self.data)
                    else: self.send(b'\x41' + object_id + struct.pack('<I', len(self.data)))
                elif c & 0xe0 == 0x60:
                    part = self.data[upload_offset:upload_offset + 7]; upload_offset += len(part)
                    toggle = (c & 0x10) ^ (0x10 if self.bad_toggle else 0)
                    self.send(bytes([toggle | ((7 - len(part)) << 1) | (upload_offset == len(self.data))]) + part)
                elif c & 0xe0 == 0x20:
                    self.send(b'\x60' + data[1:4])
                    if c & 2: self.received.append(('write', data[4:8 - ((c >> 2) & 3)]))
                    else: download = bytearray()
        except Exception as error: self.error.append(repr(error))

    def call(self, action, params=None, ok=True):
        p = subprocess.run([str(BINARY), str(self.manifest), self.temp.name],
            input=json.dumps({'action': action, 'params': params or {}, 'confirmed': True}),
            text=True, capture_output=True, timeout=15)
        result = json.loads(p.stdout); self.assertEqual(result['ok'], ok, result)
        self.assertEqual(p.returncode, 0 if ok else 1, p.stderr)
        return result.get('result', result)

    def test_expedited_and_segmented_upload_download(self):
        for data in [b'\x12', b'\x12\x34', b'\xff' * 4, self.data, b'12345678']:
            with self.subTest(length=len(data)):
                self.data = data
                value = self.call('canopen_sdo_read', {'node_id': 1, 'index': 0x2000})
                self.assertEqual(bytes.fromhex(value['data']), data)
                self.call('canopen_sdo_write', {'node_id': 1, 'index': 0x2000, 'data': data.hex()})
                self.assertIn(('write', data), self.received)

    def test_toggle_object_abort_and_no_slave_fail_closed(self):
        self.bad_toggle = True
        self.assertEqual(self.call('canopen_sdo_read', {'node_id': 1, 'index': 0x2000}, ok=False)['code'], 'InvalidResponseError')
        self.bad_toggle = False; self.wrong_object = True
        self.assertEqual(self.call('canopen_sdo_read', {'node_id': 1, 'index': 0x2000}, ok=False)['code'], 'InvalidResponseError')
        self.wrong_object = False; self.abort = True
        self.assertEqual(self.call('canopen_sdo_read', {'node_id': 1, 'index': 0x2000}, ok=False)['code'], 'SdoAbortedError')
        self.assertEqual(self.call('canopen_sdo_read', {'node_id': 2, 'index': 0x2000}, ok=False)['code'], 'TimeoutError')

    def test_scan_and_nmt_delivery_are_not_device_acknowledgements(self):
        self.data = b'\x01\x00\x00\x00'
        result = self.call('canopen_scan', {'limit': 3, 'wait': .1})
        self.assertEqual(result['nodes'], [1])
        result = self.call('canopen_nmt', {'node_id': 1, 'state': 'PRE-OPERATIONAL'})
        self.assertTrue(result['queued']); self.assertFalse(result['device_acknowledged'])
        self.assertIn(('nmt', b'\x80\x01'), self.received)

    def test_pdo_mapping_transmit_and_changed_mapping_rejection(self):
        self.objects={(0x1400,1):struct.pack('<I',0x201),(0x1400,2):b'\xff',
                      (0x1600,0):b'\x01',(0x1600,1):struct.pack('<I',0x20000010)}
        result=self.call('canopen_pdo_read',{'node_id':1,'tpdo':[],'rpdo':[1]})
        mapping=result['rpdo'][0]
        self.assertEqual(mapping['variables'],[{'index':0x2000,'subindex':0,'length':16}])
        params={'node_id':1,'pdo':1,'mapping':mapping,'data':'0600'}
        result=self.call('canopen_pdo_transmit',params)
        self.assertTrue(result['queued']);self.assertFalse(result['device_acknowledged'])
        self.assertIn(('pdo',b'\x06\x00'),self.received)
        self.objects[(0x1400,1)]=struct.pack('<I',0x80000201)
        result=self.call('canopen_pdo_transmit',params,ok=False)
        self.assertIn('mapping changed',result['error'])
        self.assertEqual(sum(kind=='pdo' for kind,_ in self.received),1)

if __name__ == '__main__': unittest.main()
