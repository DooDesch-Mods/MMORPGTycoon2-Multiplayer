"""Compile and execute the native state protocol fixture without loading a game."""
from datetime import datetime,timezone
from pathlib import Path
import hashlib
import json
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
out=root/'test/software_stream_evidence'
out.mkdir(exist_ok=True)
stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
log=out/f'fixture_{stamp}.log'
try:
    result=subprocess.run(['cmd.exe','/d','/c',str(root/'test/compile_software_stream.cmd')],cwd=root,
                          capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
    log.write_text(result.stdout+result.stderr,encoding='utf-8')
    code=result.returncode
except subprocess.TimeoutExpired as error:
    output=error.stdout or b''
    if isinstance(output,bytes): output=output.decode('utf-8',errors='replace')
    log.write_text(output+'\nerror: protocol fixture compiler/runtime exceeded 60 seconds\n',encoding='utf-8')
    code=1
record={'passed':code==0,'exit_code':code,'log':str(log),'scope':'Production protocol with isolated native and socket backends; actual game acceptance pending',
        'sources':{path:hashlib.sha256((root/path).read_bytes()).hexdigest() for path in ['src/net/software_sync.cpp','include/mt2mp/net/software_sync.h','test/software_stream_fixture.cpp']}}
(out/f'checks_{stamp}.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
print(json.dumps(record))
if code: print('\n'.join(log.read_text(encoding='utf-8').splitlines()[-30:]))
sys.exit(code)
