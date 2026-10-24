#!/usr/bin/python3
import sys,json,os,time,signal
p=os.environ['AO_JOURNAL']
def log(x):
 with open(p,'a') as f:f.write(json.dumps(x)+'\n');f.flush();os.fsync(f.fileno())
signal.signal(signal.SIGTERM,lambda *a:log({'event':'TERM','monotonic':time.monotonic()}))
log({'event':'start','pid':os.getpid(),'monotonic':time.monotonic()})
print(json.dumps({'protocol':1,'event':'status','available':True,'state':'idle','capabilities':['suspend-during-prepare','visibility-state']}),flush=True)
for l in sys.stdin:log({'event':'command','command':json.loads(l)['command']})
log({'event':'EOF','monotonic':time.monotonic()})
while True:time.sleep(.1)
