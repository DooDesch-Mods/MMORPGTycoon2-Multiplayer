"""Apply the reviewed exact Move/Remove Undo owner candidate."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
ROOT = Path(__file__).resolve().parents[1]
stage = ROOT / 'tools/move_hook_interop'
proof = json.loads((stage / 'evidence/1b53ea30-21c9-4d4f-b912-a0a78f2a1d7e/receipt.json').read_text(encoding='utf-8'))
if proof.get('passed') is not True or proof.get('checks') != 50: raise RuntimeError('interop proof differs')
for name, digest in proof['source_sha256'].items():
    if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest: raise RuntimeError('tested source changed: ' + name)
manifest = json.loads((stage / 'manifest.json').read_text(encoding='utf-8'))
prepared = []
for name, record in manifest.items():
    target = ROOT / name; candidate = ROOT / record['candidate']
    data = candidate.read_bytes()
    if hashlib.sha256(data).hexdigest() != record['candidate_sha256']: raise RuntimeError('candidate changed: ' + name)
    base = record['base_sha256']
    if base is None:
        if target.exists(): raise RuntimeError('new target exists: ' + name)
    elif hashlib.sha256(target.read_bytes()).hexdigest() != base: raise RuntimeError('Root base changed: ' + name)
    prepared.append((target,data))
backup = ROOT / 'artifacts/move_hook_interop' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
for target, data in prepared:
    if target.exists():
        retained = backup / target.relative_to(ROOT); retained.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(target,retained)
    target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(data)
backup.mkdir(parents=True,exist_ok=True)
(backup / 'receipt.json').write_text(json.dumps({'candidate':'remove_owned_open_interop_v1','files':manifest,'proof':proof['log']},indent=2),encoding='utf-8')
print('info: Move/Remove owned Undo route integrated; native proof pending')
