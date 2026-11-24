#!/usr/bin/env python3
"""Server-side Tuya Cloud adapter for the desktop's restricted home API.

Keep the project Access Secret on the gateway host. Configure only explicitly
authorized devices/codes. No command retries, including after ambiguous failures.
API signing: https://developer.tuya.com/en/docs/iot/new-singnature?id=Kbw0q34cs2e5g
"""
import hashlib,hmac,json,os,re,sqlite3,ssl,stat,threading,time
from pathlib import Path
from urllib.request import Request,build_opener,HTTPSHandler,HTTPRedirectHandler,ProxyHandler
from flask import Flask,request,jsonify

ENDPOINTS={'cn':'https://openapi.tuyacn.com','us':'https://openapi.tuyaus.com',
           'eu':'https://openapi.tuyaeu.com','in':'https://openapi.tuyain.com',
           'us-east':'https://openapi-ueaz.tuyaus.com','eu-west':'https://openapi-weaz.tuyaeu.com'}
ID=re.compile(r'^[A-Za-z0-9_-]{1,128}$')

def private_file(path):
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_CLOEXEC)
    try:
        info=os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.geteuid() or info.st_mode&0o077 or info.st_size>32768:
            raise ValueError('Owner-only regular configuration required')
        return os.read(fd,32769).decode().strip()
    finally:os.close(fd)

class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs):raise ValueError('Tuya redirect refused')

def signature(client_id,secret,token,timestamp,method,path,body):
    content_hash=hashlib.sha256(body).hexdigest()
    canonical=method+'\n'+content_hash+'\n\n'+path
    return hmac.new(secret.encode(),(client_id+token+timestamp+canonical).encode(),hashlib.sha256).hexdigest().upper()

class Tuya:
    def __init__(self,config):
        self.endpoint=ENDPOINTS[config['region']]
        self.client_id=config['access_id'];self.secret=private_file(config['access_secret_file'])
        if not ID.fullmatch(self.client_id) or not self.secret:raise ValueError('Invalid project credential')
        self.token='';self.expires=0;self.lock=threading.Lock()
        self.http=build_opener(ProxyHandler({}),HTTPSHandler(context=ssl.create_default_context()),NoRedirect())
    def _call(self,method,path,body=b'',token=''):
        stamp=str(int(time.time()*1000))
        headers={'client_id':self.client_id,'sign_method':'HMAC-SHA256','t':stamp,
            'sign':signature(self.client_id,self.secret,token,stamp,method,path,body),'Content-Type':'application/json'}
        if token:headers['access_token']=token
        req=Request(self.endpoint+path,data=body if method=='POST' else None,headers=headers,method=method)
        with self.http.open(req,timeout=8) as response:
            raw=response.read(524289)
            if len(raw)>524288:raise ValueError('Tuya response too large')
        result=json.loads(raw)
        if result.get('success') is not True:raise RuntimeError('Tuya API rejected the request')
        return result['result']
    def call(self,method,path,body=None):
        with self.lock:
            if time.monotonic()>=self.expires:
                result=self._call('GET','/v1.0/token?grant_type=1')
                ttl=result['expire_time']
                if not isinstance(ttl,int) or ttl<60 or not isinstance(result['access_token'],str):raise ValueError('Invalid Tuya token response')
                self.token=result['access_token'];self.expires=time.monotonic()+ttl-30
            token=self.token
        raw=json.dumps(body,separators=(',',':'),ensure_ascii=False).encode() if body is not None else b''
        return self._call(method,path,raw,token)

def create_app(config,client=None):
    app=Flask(__name__);app.config['MAX_CONTENT_LENGTH']=8192
    api=client or Tuya(config)
    token=private_file(config['desktop_token_file'])
    if not re.fullmatch(r'[A-Za-z0-9._~-]{32,1024}',token):raise ValueError('Invalid restricted desktop token')
    allowed=config['devices']
    if not isinstance(allowed,dict) or len(allowed)>64:raise ValueError('Explicit device authorization required')
    for ident,codes in allowed.items():
        if not ID.fullmatch(ident) or not isinstance(codes,list) or len(codes)>24 or any(not isinstance(c,str) or not ID.fullmatch(c) for c in codes) or len(set(codes))!=len(codes):raise ValueError('Invalid device permission')
    database=Path(config['command_database'])
    if not database.parent.is_dir() or database.parent.stat().st_uid!=os.geteuid() or database.parent.stat().st_mode&0o077 or database.is_symlink():
        raise ValueError('Private gateway state directory required')
    with sqlite3.connect(database) as db:db.execute('CREATE TABLE IF NOT EXISTS commands (key TEXT PRIMARY KEY, digest TEXT NOT NULL, state TEXT NOT NULL)')
    database.chmod(0o600)
    @app.before_request
    def authorize():
        if not hmac.compare_digest(request.headers.get('Authorization',''),'Bearer '+token):return jsonify(error='unauthorized'),403
    @app.errorhandler(Exception)
    def failed(error):
        # Never return upstream bodies, signatures, tokens or exception text.
        return jsonify(error='provider_failed',state_confirmed=False),502
    def device(ident):
        if ident not in allowed:raise PermissionError('Device not authorized')
        info=api.call('GET','/v1.0/devices/'+ident)
        if info.get('id')!=ident or type(info.get('online')) is not bool:raise ValueError('Invalid device identity')
        return info
    def functions(ident):
        if ident not in allowed:raise PermissionError('Device not authorized')
        result=api.call('GET','/v1.0/devices/'+ident+'/functions')
        values=result['functions']
        if not isinstance(values,list) or len(values)>128:raise ValueError('Invalid functions')
        supported={'boolean':'Boolean','integer':'Integer','enum':'Enum'}
        result=[];seen=set()
        for value in values:
            code=value.get('code');kind=supported.get(str(value.get('type','')).lower())
            if code not in allowed[ident] or kind is None:continue
            if code in seen:raise ValueError('Duplicate capability')
            seen.add(code)
            limits=value.get('values',{})
            if isinstance(limits,str):limits=json.loads(limits)
            if not isinstance(limits,dict):raise ValueError('Invalid capability schema')
            if kind=='Integer' and not (all(type(limits.get(k)) is int for k in ('min','max','step')) and -2147483648<=limits['min']<=limits['max']<=2147483647 and limits['step']>0):raise ValueError('Invalid integer capability')
            if kind=='Enum' and not (isinstance(limits.get('range'),list) and len(limits['range'])<=64 and all(isinstance(v,str) for v in limits['range'])):raise ValueError('Invalid enum capability')
            result.append(dict(value,type=kind,values=limits))
        return result
    @app.get('/v1/devices')
    def devices():
        result=[]
        for ident in allowed:
            info=device(ident)
            result.append({'id':ident,'name':info.get('name',ident),'room':'','online':info['online'],'status':info.get('status',[])})
        return jsonify(devices=result)
    @app.get('/v1/devices/<ident>/functions')
    def capabilities(ident):
        if ident not in allowed:return jsonify(error='unauthorized'),403
        return jsonify(control_allowed=bool(allowed[ident]),functions=functions(ident))
    @app.post('/v1/devices/<ident>/commands')
    def commands(ident):
        if ident not in allowed:return jsonify(error='unauthorized'),403
        body=request.get_json();key=request.headers.get('Idempotency-Key','')
        if not re.fullmatch(r'[A-Za-z0-9-]{16,64}',key) or not isinstance(body,dict) or set(body)!={'code','value'}:
            return jsonify(error='invalid_request'),400
        code=body['code'];value=body['value']
        if not isinstance(code,str) or code not in allowed[ident]:return jsonify(error='unauthorized'),403
        digest=hashlib.sha256((ident+json.dumps(body,sort_keys=True,separators=(',',':'))).encode()).hexdigest()
        # Reserve BEFORE all upstream work. A crash/timeout never grants a replay.
        with sqlite3.connect(database) as db:
            try:db.execute('INSERT INTO commands VALUES (?,?,?)',(key,digest,'unknown'))
            except sqlite3.IntegrityError:return jsonify(error='duplicate_or_outcome_unknown',state_confirmed=False),409
        info=device(ident)
        if not info['online']:return jsonify(error='device_offline'),409
        capability=next((v for v in functions(ident) if v['code']==code),None)
        if capability is None:return jsonify(error='capability_unavailable'),409
        limits=capability.get('values',{})
        if isinstance(limits,str):limits=json.loads(limits)
        kind=capability.get('type','').lower()
        valid=(kind=='boolean' and type(value) is bool)
        if kind=='integer':valid=type(value) is int and limits['min']<=value<=limits['max'] and (value-limits['min'])%max(1,limits.get('step',1))==0
        if kind=='enum':valid=isinstance(value,str) and value in limits.get('range',[])
        if not valid:return jsonify(error='invalid_value'),400
        # The desktop contract is one command; Tuya Cloud wraps it in an array.
        result=api.call('POST','/v1.0/devices/'+ident+'/commands',{'commands':[body]})
        if result is not True:raise RuntimeError('Command not accepted')
        with sqlite3.connect(database) as db:db.execute('UPDATE commands SET state=? WHERE key=?',('accepted',key))
        return jsonify(accepted=True,state_confirmed=False)
    return app

def configured_app():
    return create_app(json.loads(private_file(os.environ['DSHANCONTROL_TUYA_GATEWAY_CONFIG'])))
