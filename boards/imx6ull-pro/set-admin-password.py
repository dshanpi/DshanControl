"""Development-board provisioning helper. Generates a private, per-board admin password."""
import hashlib,json,os,secrets,tempfile
from pathlib import Path
path=Path('/data/dshancontrol/data/web.json')
if os.geteuid()!=0 or path.is_symlink() or not path.is_file():
    raise SystemExit('Requires root and an existing regular board Web config')
value=json.loads(path.read_text());password=secrets.token_urlsafe(18);salt=secrets.token_hex(16)
value['auth']={'username':'admin','role':'administrator','salt':salt,'password_hash':hashlib.pbkdf2_hmac('sha256',password.encode(),salt.encode(),120000).hex(),'must_change':True}
with tempfile.NamedTemporaryFile(mode='w',dir=path.parent,prefix='.admin-',delete=False) as f:
    temporary=Path(f.name);os.fchmod(f.fileno(),0o600);json.dump(value,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
try:
    os.replace(temporary,path)
    fd=os.open(path.parent,os.O_DIRECTORY);os.fsync(fd);os.close(fd)
finally:
    if temporary.exists():temporary.unlink()
print(json.dumps({'username':'admin','password':password,'must_change':True}))
