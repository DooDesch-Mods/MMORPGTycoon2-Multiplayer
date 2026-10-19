"""Copy the fully reviewed native time-button entry under its frozen hashes."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
manifest=ROOT/'tools/engine_analysis/time_mouse_failure/native_button/production_copy_manifest.json'
value=json.loads(manifest.read_text(encoding='utf-8'))
for item in value['copy_sources']:
    if hashlib.sha256((ROOT/item['source']).read_bytes()).hexdigest()!=item['sha256']:
        raise ValueError('native button source hash differs: '+item['source'])
    if (ROOT/item['target']).exists():raise ValueError('native button target already exists: '+item['target'])
out=ROOT/'artifacts/time_mouse_integration'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out.mkdir(parents=True,exist_ok=False)
receipt={'candidate':value['candidate'],'copies':[],'passed':False}
try:
    for item in value['copy_sources']:
        target=ROOT/item['target'];target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes((ROOT/item['source']).read_bytes())
        actual=hashlib.sha256(target.read_bytes()).hexdigest()
        receipt['copies'].append({**item,'actual_sha256':actual})
        if actual!=item['sha256']:raise ValueError('native button copied hash differs: '+item['target'])
    receipt['passed']=True
except Exception as error:
    receipt['error']=str(error);raise
finally:
    (out/'copy_receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print('info: native button copy passed='+str(receipt['passed'])+' receipt='+str(out/'copy_receipt.json'))
