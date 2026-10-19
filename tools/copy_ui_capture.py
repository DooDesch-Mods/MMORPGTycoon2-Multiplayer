"""Copy the reviewed UI entry candidate with immutable source hashes."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'tools/action_session/ui_capture/entry_review_delta'
frozen = json.loads((BASE / 'frozen.json').read_text(encoding='utf-8'))
names = ('ui_capture.h', 'ui_capture.cpp', 'native_contract.h', 'native_contract.cpp',
         'native_contract_bytes.h', 'native_entry_windows.h', 'native_entry_windows.cpp', 'entry_dispatch.cpp')
target = ROOT / 'src/game/ui_remove'
for name in names:
    if hashlib.sha256((BASE / name).read_bytes()).hexdigest() != frozen['sources'][name]:
        raise ValueError('UI capture frozen source changed: ' + name)
    if (target / name).exists():
        raise ValueError('UI capture target already exists: ' + name)
out = ROOT / 'artifacts/ui_remove_integration' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out.mkdir(parents=True, exist_ok=False)
target.mkdir(parents=True, exist_ok=True)
receipt = {'candidate': frozen['candidate'], 'passed': False, 'copies': []}
try:
    for name in names:
        (target / name).write_bytes((BASE / name).read_bytes())
        actual = hashlib.sha256((target / name).read_bytes()).hexdigest()
        receipt['copies'].append({'target': str(target / name), 'sha256': actual})
        if actual != frozen['sources'][name]:
            raise ValueError('UI capture target hash differs: ' + name)
    receipt['passed'] = True
except Exception as error:
    receipt['error'] = str(error)
    raise
finally:
    (out / 'copy_receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(f'info: UI capture copy passed={receipt["passed"]} receipt={out}/copy_receipt.json')
