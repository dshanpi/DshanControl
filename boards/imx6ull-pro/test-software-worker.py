import socket,subprocess,threading,tempfile,os,json,time,hashlib,struct,sys,pathlib
binary=sys.argv[1];clip=sys.argv[2];rows=[]
with tempfile.TemporaryDirectory() as td:
 path=td+'/frames.sock';server=socket.socket(socket.AF_UNIX);server.bind(path);server.listen()
 p=subprocess.Popen([binary],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=open(td+'/stderr','w'),text=True,env=dict(os.environ,DSHANCONTROL_MEDIA_FRAME_SOCKET=path))
 conn,_=server.accept();states=[];replies={};frames=[];running=True
 def statuses():
  for line in p.stdout:
   v=json.loads(line)
   if v['event']=='status':states.append(v)
   else:replies[v['request_id']]=v
 def pixels():
  data=b''
  while running:
   chunk=conn.recv(65536)
   if not chunk:break
   data+=chunk
   while len(data)>=20:
    magic,w,h,n,g=struct.unpack('<5I',data[:20]);assert magic==0x31465244 and 0<w<=640 and 0<h<=480 and n==w*h*2
    if len(data)<20+n:break
    frames.append((g,hashlib.sha256(data[20:20+n]).hexdigest(),w,h));data=data[20+n:]
 threading.Thread(target=statuses,daemon=True).start();threading.Thread(target=pixels,daemon=True).start()
 def wait(test,timeout=8):
  end=time.monotonic()+timeout
  while time.monotonic()<end:
   if test():return
   time.sleep(.02)
  raise AssertionError(('timeout',states[-3:],replies))
 def request(cmd,params={},accept=True):
  key=str(len(replies)+1);p.stdin.write(json.dumps({'protocol':1,'request_id':key,'command':cmd,'params':params})+'\n');p.stdin.flush();wait(lambda:key in replies)
  assert replies[key]['accepted']==accept,(cmd,replies[key]);rows.append({'test':cmd,'accepted':accept});return replies[key]
 try:
  wait(lambda:states and states[-1]['available']);request('rect',{'x':0,'y':0,'width':640,'height':360})
  request('open',{'path':clip,'volume':60});request('suspend');time.sleep(.5);request('snapshot');assert states[-1]['state'] in ('prepared','paused') and states[-1]['visible'] is False;request('visible',{'value':True});request('play');wait(lambda:len(frames)>=20);assert len(set(x[1] for x in frames))>=10
  request('rect',{'x':0,'y':0,'width':160,'height':90});wait(lambda:frames[-1][2:]==(160,90));assert states[-1]['width']==640 and states[-1]['height']==360
  request('rect',{'x':0,'y':0,'width':640,'height':360});wait(lambda:frames[-1][2:]==(320,180))
  assert states[-1]['hardware_decode_confirmed'] is False and states[-1]['audio_available'] is False
  request('pause');time.sleep(.3);request('snapshot');a=states[-1]['position_ms'];time.sleep(.6);request('snapshot');assert abs(states[-1]['position_ms']-a)<100
  request('seek',{'position_ms':10000});wait(lambda:states[-1].get('position_ms',0)>=9900 and not states[-1].get('seeking'))
  request('play');wait(lambda:states[-1]['state']=='playing');request('suspend');assert states[-1]['visible'] is False and states[-1]['state']=='paused'
  request('visible',{'value':True});assert states[-1]['state']=='paused';request('play')
  request('seek',{'position_ms':-1},False);request('volume',{'value':50},False);request('loop',{'value':True})
  request('seek',{'position_ms':19500});time.sleep(1.5);request('snapshot');assert states[-1]['state']=='playing' and states[-1]['position_ms']<5000
  request('loop',{'value':False});request('seek',{'position_ms':19500});wait(lambda:states[-1]['state']=='completed')
  request('close');assert states[-1]['state']=='idle' and states[-1]['first_frame'] is False
  bad=td+'/invalid.mp4';pathlib.Path(bad).write_bytes(b'invalid media')
  request('open',{'path':bad,'volume':60});wait(lambda:states[-1]['state']=='error');time.sleep(.5);assert states[-1]['state']=='error'
  os.symlink(clip,td+'/link.mp4');request('open',{'path':td+'/link.mp4','volume':60},False)
  os.mkfifo(td+'/fifo');request('open',{'path':td+'/fifo','volume':60},False)
  request('open',{'path':clip,'volume':60});wait(lambda:states[-1]['state']=='playing');request('close')
  result={'status':'passed','checks':rows,'frames':len(frames),'different_frame_hashes':len(set(x[1] for x in frames)),'software_decode_only':True,'stderr':pathlib.Path(td+'/stderr').read_text()[-1500:]};print(json.dumps(result,indent=2))
 finally:
  running=False;p.stdin.close()
  try:p.wait(timeout=3)
  except subprocess.TimeoutExpired:p.kill();p.wait()
  conn.close();server.close()
