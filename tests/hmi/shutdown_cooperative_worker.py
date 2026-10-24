#!/usr/bin/python3
import sys,json,os,time
p=os.environ['AO_JOURNAL']
def log(x):
 with open(p,'a') as f:f.write(json.dumps(x)+'\n');f.flush();os.fsync(f.fileno())
log({'event':'start','pid':os.getpid()})
print(json.dumps({'protocol':1,'event':'status','available':True,'state':'idle','capabilities':['suspend-during-prepare','visibility-state']}),flush=True)
for l in sys.stdin:
 q=json.loads(l);log({'event':'command','command':q['command']})
log({'event':'EOF'});log({'event':'normal-exit'})
