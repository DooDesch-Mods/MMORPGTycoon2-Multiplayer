"""Copy the reviewed native UI helper and retain the sole include adaptation."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'tools/action_session/ui_capture/diagnostic_entry'
frozen=json.loads((BASE/'frozen.json').read_text(encoding='utf-8'))
names=('ui_diagnostic.h','ui_diagnostic.cpp','process_calls.cpp','native_contract_bytes.h')
target=ROOT/'src/game/ui_remove/diagnostic'
for name in names:
    if hashlib.sha256((BASE/name).read_bytes()).hexdigest()!=frozen['sources'][name]:
        raise ValueError('native UI helper frozen source changed: '+name)
    if (target/name).exists():raise ValueError('native UI helper target exists: '+name)
out=ROOT/'artifacts/ui_diagnostic_integration'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out.mkdir(parents=True,exist_ok=False);target.mkdir(parents=True,exist_ok=True)
receipt={'candidate':frozen['candidate'],'passed':False,'copies':[]}
try:
    for name in names:
        data=(BASE/name).read_bytes()
        if name=='ui_diagnostic.h':
            before=b'../entry_review_delta/native_contract.h'
            if data.count(before)!=1:raise ValueError('native UI helper include adaptation is ambiguous')
            data=data.replace(before,b'../native_contract.h')
        (target/name).write_bytes(data)
        actual=hashlib.sha256((target/name).read_bytes()).hexdigest()
        receipt['copies'].append({'target':str(target/name),'source_sha256':frozen['sources'][name],
            'sha256':actual,'adaptation':'Root include' if name=='ui_diagnostic.h' else None})
        if actual!=hashlib.sha256(data).hexdigest():raise ValueError('native UI helper copy differs: '+name)
    receipt['passed']=True
except Exception as error:
    receipt['error']=str(error);raise
finally:
    (out/'copy_receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print(f'info: native UI helper copy passed={receipt["passed"]} receipt={out}/copy_receipt.json')
