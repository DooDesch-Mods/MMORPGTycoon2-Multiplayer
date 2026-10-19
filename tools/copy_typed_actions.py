"""Copy the corrected frozen controller after Root's source review."""
from pathlib import Path
import hashlib
import json
from datetime import datetime, timezone

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'tools/action_session/review_delta'
frozen=json.loads((BASE/'frozen.json').read_text(encoding='utf-8'))
names=('action_session.h','action_session.cpp','sidecar_adapter.h','sidecar_adapter.cpp')
for name in names:
    if hashlib.sha256((BASE/name).read_bytes()).hexdigest()!=frozen['sources'][name]:
        raise ValueError('typed action frozen source changed: '+name)
    if (ROOT/'src/game/typed_action'/name).exists():
        raise ValueError('typed action target already exists: '+name)
target=ROOT/'src/game/typed_action';target.mkdir(parents=True,exist_ok=True)
receipt={'candidate':frozen['candidate'],'passed':False,'copies':[]}
out=ROOT/'artifacts/action_integration'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out.mkdir(parents=True,exist_ok=False)
try:
    for name in names:
        (target/name).write_bytes((BASE/name).read_bytes())
        actual=hashlib.sha256((target/name).read_bytes()).hexdigest()
        receipt['copies'].append({'target':str(target/name),'sha256':actual})
        if actual!=frozen['sources'][name]:
            raise ValueError('typed action copy differs: '+name)
    receipt['passed']=True
except Exception as error:
    receipt['error']=str(error);raise
finally:
    (out/'copy_receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print(f'info: typed action copy passed={receipt["passed"]} receipt={out}/copy_receipt.json')
