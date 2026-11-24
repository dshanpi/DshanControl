#!/usr/bin/env python3
"""Exchange simulated EtherCAT BRD/AL-status datagrams on Linux loopback only.

Requires CAP_NET_RAW. This exercises real AF_PACKET frames and WKC validation,
not SOEM slave discovery, CoE/PDO mapping, distributed clocks or real-time timing.
No physical interface, actuator or production provider is used.
"""
import json,socket,struct,threading,time

ETHERTYPE=0x88a4
def packet(index,status=0,wkc=0):
    datagram=struct.pack('<BBHHHHHH',7,index,0,0x0130,2,0,status,wkc)
    return bytes.fromhex('ffffffffffff02000000000188a4')+struct.pack('<H',0x1000|len(datagram))+datagram

def decode(frame):
    if len(frame)<30 or frame[12:14]!=b'\x88\xa4':return None
    header=struct.unpack_from('<H',frame,14)[0]
    if header&0xf800!=0x1000 or header&0x7ff!=14:return None
    cmd,index,adp,ado,length,irq,status,wkc=struct.unpack_from('<BBHHHHHH',frame,16)
    if (cmd,adp,ado,length,irq)!=(7,0,0x0130,2,0):return None
    return {'index':index,'al_status':status,'work_counter':wkc}

def main():
    errors=[];observations=[];stop=threading.Event()
    with socket.socket(socket.AF_PACKET,socket.SOCK_RAW,socket.htons(ETHERTYPE)) as master, socket.socket(socket.AF_PACKET,socket.SOCK_RAW,socket.htons(ETHERTYPE)) as slave:
        for stream in (master,slave):stream.bind(('lo',0));stream.settimeout(.2)
        def respond():
            while not stop.is_set():
                try:frame,address=slave.recvfrom(2048)
                except socket.timeout:continue
                except OSError as error:errors.append(str(error));return
                value=decode(frame)
                if address[2]!=socket.PACKET_OUTGOING and value and value['work_counter']==0:
                    slave.send(packet(value['index'],4,1))
        thread=threading.Thread(target=respond);thread.start()
        try:
            for index in range(80,85):
                started=time.monotonic();master.send(packet(index));deadline=started+2
                received=None
                while time.monotonic()<deadline:
                    try:frame,address=master.recvfrom(2048)
                    except socket.timeout:continue
                    value=decode(frame)
                    if value and value['index']==index and value['work_counter']==1:
                        received=value;break
                assert received is not None and received['al_status']==4,'Simulated EtherCAT response missing/invalid'
                observations.append(dict(received,elapsed_ms=round((time.monotonic()-started)*1000,3)))
        finally:stop.set();thread.join(2)
    assert not errors,errors
    print(json.dumps({'passed':True,'scope':'software-datagram-simulation','interface':'lo','ethertype':'0x88a4',
                      'physical_slave':False,'soem_discovery_tested':False,'observations':observations}))

if __name__=='__main__':main()
