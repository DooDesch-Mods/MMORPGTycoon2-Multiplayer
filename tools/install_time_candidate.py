"""Copy the hash-pinned time integration into the owned build workspace."""
import hashlib
import json
from pathlib import Path
import shutil
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
contract_path = ROOT / 'tools/engine_analysis/calendar_authority/time_integration/evidence/contract.json'
contract = json.loads(contract_path.read_text(encoding='utf-8'))
assert contract['passed'] and len(contract['copy_manifest']) == 9
entries = contract['copy_manifest']
for entry in entries:
    source = ROOT / entry['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == entry['sha256'], f'Frozen source changed: {source}'
    (ROOT / entry['target']).resolve().relative_to((ROOT / 'src/game').resolve())
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
directory = ROOT / 'artifacts/time_integration' / stamp
directory.mkdir(parents=True, exist_ok=False)
record = {'candidate': 'TIME-INTEGRATION-1', 'contract_sha256': hashlib.sha256(contract_path.read_bytes()).hexdigest(),
          'files': [], 'native_runtime_proven': False}
for entry in entries:
    source, target = ROOT / entry['source'], ROOT / entry['target']
    prior = None
    if target.exists():
        prior = hashlib.sha256(target.read_bytes()).hexdigest()
        shutil.copyfile(target, directory / (target.name + '.prior'))
    shutil.copyfile(source, target)
    assert hashlib.sha256(target.read_bytes()).hexdigest() == entry['sha256'], f'Copy differs: {target}'
    record['files'].append({**entry, 'prior_sha256': prior})
(directory / 'copy_receipt.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(f'info: time candidate copied files=9 receipt={directory / "copy_receipt.json"} native_runtime_proven=False')
