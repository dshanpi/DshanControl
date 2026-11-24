#!/usr/bin/env python3
"""Bounded fieldbus executor. Invoked by the root Unix service, never a web shell."""
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import time
import tempfile


def integer(value, name, low, high):
    if isinstance(value, bool) or not isinstance(value, (str, int)):
        raise ValueError(name + ': integer required')
    if isinstance(value, str):
        if not re.fullmatch(r'-?(?:0[xX][0-9a-fA-F]+|[0-9]+)', value):
            raise ValueError(name + ': invalid integer')
        value = int(value, 16 if 'x' in value.lower() else 10)
    if not low <= value <= high:
        raise ValueError(name + ': outside range')
    return value


def fields(params, allowed, required=()):
    if not isinstance(params, dict) or set(params)-set(allowed) or set(required)-set(params):
        raise ValueError('Unexpected or missing operation parameters')


def command(args, timeout=5):
    result = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, timeout=timeout)
    if len(result.stdout) > 65536 or len(result.stderr) > 8192:
        raise ValueError('Tool output exceeds limit')
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors='replace').strip()[:2000] or 'Tool failed')
    return result.stdout.decode()


class Fieldbus:
    def __init__(self, manifest, state):
        self.manifest, self.state = manifest, Path(state)

    def device(self, kind, requested=None, role=None):
        items = [v['device'] for v in self.manifest['channels'].values()
                 if v['kind'] == kind and (role is None or v.get('role') == role)]
        if role and role not in self.manifest.get('capabilities', []):
            raise ValueError('Capability not declared')
        if requested is None and len(items) == 1: requested = items[0]
        if requested not in items: raise ValueError('Device is not mapped to this board')
        return requested

    def execute(self, action, p):
        if action == 'can_apply':
            fields(p, {'interface','bitrate','restart_ms','fd','data_bitrate','loopback'}, {'bitrate'})
            device = self.device('can', p.get('interface'))
            bitrate = integer(p['bitrate'],'bitrate',10000,1000000)
            restart = integer(p.get('restart_ms',100),'restart_ms',0,60000)
            fd, loopback = p.get('fd',False), p.get('loopback',False)
            if type(fd) is not bool or type(loopback) is not bool: raise ValueError('Boolean required')
            if fd and 'can-fd' not in self.manifest.get('capabilities',[]):
                raise ValueError('CAN-FD is not supported by this board')
            extra = ['dbitrate',str(integer(p.get('data_bitrate'),'data_bitrate',bitrate,8000000)),'fd','on'] if fd else []
            command(['/sbin/ip','link','set','dev',device,'down'])
            command(['/sbin/ip','link','set','dev',device,'type','can','bitrate',str(bitrate),
                     'restart-ms',str(restart),'loopback','on' if loopback else 'off']+extra)
            command(['/sbin/ip','link','set','dev',device,'up'])
            return {'interface':device,'bitrate':bitrate,'fd':fd,'loopback':loopback,
                    'driver':json.loads(command(['/sbin/ip','-details','-json','link','show','dev',device]))}
        if action in ('can_send','can_receive'):
            fields(p, {'interface','id','data','extended','timeout_ms'}, {'id','data'} if action=='can_send' else ())
            device = self.device('can',p.get('interface'))
            timeout = integer(p.get('timeout_ms',1000),'timeout_ms',10,3000)/1000
            with socket.socket(socket.PF_CAN,socket.SOCK_RAW,socket.CAN_RAW) as bus:
                bus.settimeout(timeout); bus.bind((device,))
                if action == 'can_send':
                    extended = p.get('extended',False)
                    if type(extended) is not bool: raise ValueError('extended must be boolean')
                    identifier = integer(p['id'],'id',0,0x1fffffff if extended else 0x7ff)
                    data = p['data']
                    if not isinstance(data,str) or not re.fullmatch(r'(?:[0-9a-fA-F]{2}){0,8}',data):
                        raise ValueError('CAN data must contain 0..8 hex bytes')
                    payload = bytes.fromhex(data)
                    frame = struct.pack('=IB3x8s',identifier | (0x80000000 if extended else 0),len(payload),payload)
                    if bus.send(frame) != len(frame): raise RuntimeError('Incomplete CAN send')
                    return {'interface':device,'queued':True,'device_acknowledged':False,'bytes':len(payload)}
                frame = bus.recv(16)
                identifier,length,data = struct.unpack('=IB3x8s',frame)
                return {'interface':device,'id':identifier&0x1fffffff,'flags':identifier&0xe0000000,
                        'data':data[:length].hex(),'timestamp':time.time()}
        if action.startswith('canopen_'):
            if 'canopen' not in self.manifest.get('capabilities',[]): raise ValueError('CANopen unavailable')
            allowed={'interface','node_id','state'} if action=='canopen_nmt' else {'interface','node_id','index','subindex','size','value'}
            if action not in ('canopen_nmt','canopen_sdo_read','canopen_sdo_write'): raise ValueError('Unknown CANopen operation')
            fields(p,allowed,{'node_id'})
            device=self.device('can',p.get('interface'))
            node_id=integer(p['node_id'],'node_id',0 if action=='canopen_nmt' else 1,127)
            if action=='canopen_nmt':
                state=p.get('state')
                if state not in ('OPERATIONAL','PRE-OPERATIONAL','STOPPED','RESET','RESET COMMUNICATION'):
                    raise ValueError('Invalid NMT state')
            else:
                index=integer(p.get('index'),'index',0,65535)
                subindex=integer(p.get('subindex',0),'subindex',0,255)
                size=integer(p.get('size',4),'size',1,4)
                if action=='canopen_sdo_write':
                    value=integer(p.get('value'),'value',-(1<<(size*8-1)),(1<<(size*8))-1)
                    payload=value.to_bytes(size,'little',signed=value<0)
            import canopen
            bus=canopen.Network()
            bus.connect(channel=device,interface='socketcan')
            try:
                if action=='canopen_nmt':
                    (bus.nmt if node_id==0 else bus.add_node(node_id).nmt).state=state
                    return {'node_id':node_id,'state':state,'queued':True,'device_acknowledged':False}
                node=bus.add_node(node_id)
                node.sdo.RESPONSE_TIMEOUT=1.0
                node.sdo.MAX_RETRIES=1
                if action=='canopen_sdo_read':
                    data=node.sdo.upload(index,subindex)
                    if len(data)>4096: raise ValueError('SDO response too large')
                    return {'node_id':node_id,'index':index,'subindex':subindex,'data':data.hex(),'device_acknowledged':True}
                node.sdo.download(index,subindex,payload)
                return {'node_id':node_id,'index':index,'subindex':subindex,'device_acknowledged':True}
            finally: bus.disconnect()
        if action.startswith('rs485_'):
            return self.modbus(action,p)
        if action.startswith('ethercat_'):
            return self.ethercat(action,p)
        raise ValueError('Unsupported fieldbus operation')

    def modbus(self, action, p):
        if action == 'rs485_config':
            fields(p,{'port','baudrate','unit_id','poll_ms','parity','stopbits'}, {'port','baudrate','unit_id','poll_ms'})
            cfg={'port':self.device('serial',p['port'],'modbus-rtu'),
                 'baudrate':integer(p['baudrate'],'baudrate',300,4000000),
                 'unit_id':integer(p['unit_id'],'unit_id',1,247),
                 'poll_ms':integer(p['poll_ms'],'poll_ms',50,3600000),
                 'parity':p.get('parity','N'),'stopbits':integer(p.get('stopbits',1),'stopbits',1,2)}
            if cfg['parity'] not in ('N','E','O'): raise ValueError('Invalid parity')
            self.state.mkdir(mode=0o700,parents=True,exist_ok=True)
            if self.state.is_symlink(): raise ValueError('State directory must not be linked')
            with tempfile.NamedTemporaryFile(mode='w',dir=self.state,prefix='.rs485-',delete=False) as output:
                temporary=Path(output.name)
                try:
                    os.fchmod(output.fileno(),0o600);json.dump(cfg,output);output.flush();os.fsync(output.fileno())
                    temporary.replace(self.state/'rs485.json')
                    directory=os.open(self.state,os.O_RDONLY|os.O_DIRECTORY)
                    try:os.fsync(directory)
                    finally:os.close(directory)
                finally:temporary.unlink(missing_ok=True)
            return dict(cfg,configured=True,polling=False,device_acknowledged=False)
        if action not in ('rs485_read','rs485_write'): raise ValueError('Unknown Modbus operation')
        fields(p,{'port','unit_id','address','count','function','value'}, {'address'})
        cfg=json.loads((self.state/'rs485.json').read_text())
        device=self.device('serial',p.get('port',cfg['port']),'modbus-rtu')
        if device != cfg['port']: raise ValueError('Serial configuration changed')
        station=integer(p.get('unit_id',cfg['unit_id']),'unit_id',1,247)
        address=integer(p['address'],'address',0,65535)
        count=integer(p.get('count',1),'count',1,125)
        if address+count>65536: raise ValueError('Register range overflows')
        function=integer(p.get('function',3 if action=='rs485_read' else 6),'function',1,16)
        if function not in ((3,4) if action=='rs485_read' else (6,)): raise ValueError('Unsupported Modbus function')
        value=integer(p.get('value'),'value',0,65535) if action=='rs485_write' else None
        import minimalmodbus
        instrument=minimalmodbus.Instrument(device,station,close_port_after_each_call=True)
        instrument.serial.baudrate=cfg['baudrate'];instrument.serial.parity=cfg['parity']
        instrument.serial.stopbits=cfg['stopbits'];instrument.serial.timeout=1.0
        if action=='rs485_read': data=instrument.read_registers(address,count,functioncode=function)
        else:
            instrument.write_register(address,value,functioncode=6);data=[value]
        return {'port':device,'unit_id':station,'address':address,'values':data,'device_acknowledged':True,'timestamp':time.time()}

    def ethercat(self, action, p):
        allowed={'interface'}
        if action in ('ethercat_sdo_read','ethercat_sdo_write'): allowed|={'slave','index','subindex','value'}
        elif action=='ethercat_cycle': allowed|={'count','period_us'}
        elif action!='ethercat_scan': raise ValueError('Unknown EtherCAT operation')
        fields(p,allowed)
        device=self.device('ethernet',p.get('interface'),'ethercat')
        args=['/usr/bin/omnigate-ethercat',device]
        if action=='ethercat_scan': args+=['scan']
        elif action=='ethercat_cycle':
            count=integer(p.get('count',100),'count',1,10000)
            period=integer(p.get('period_us',1000),'period_us',100,1000000)
            if count*period>5000000: raise ValueError('Cycle exceeds five-second budget')
            args+=['cycle',str(count),str(period)]
        else:
            args += ['sdo-read' if action=='ethercat_sdo_read' else 'sdo-write',
                     str(integer(p.get('slave'),'slave',1,65535)),hex(integer(p.get('index'),'index',0,65535)),
                     str(integer(p.get('subindex',0),'subindex',0,255))]
            if action=='ethercat_sdo_write':
                value=p.get('value')
                if not isinstance(value,str) or not re.fullmatch(r'[0-9A-Fa-f]{2}(?:\s+[0-9A-Fa-f]{2}){0,31}',value):
                    raise ValueError('Expected 1..32 hexadecimal bytes')
                args.append(value)
        return json.loads(command(args,timeout=10))


def main():
    try:
        request=json.loads(sys.stdin.buffer.read(65537))
        fields(request,{'action','params','confirmed'},{'action','params','confirmed'})
        if request['confirmed'] is not True: raise ValueError('Explicit confirmation required')
        manifest=json.loads(Path(sys.argv[1]).read_text())
        result=Fieldbus(manifest,sys.argv[2]).execute(request['action'],request['params'])
        print(json.dumps({'ok':True,'result':result}))
    except Exception as error:
        print(json.dumps({'ok':False,'code':type(error).__name__,'error':str(error)[:2000]}))
        raise SystemExit(1)


if __name__=='__main__': main()
