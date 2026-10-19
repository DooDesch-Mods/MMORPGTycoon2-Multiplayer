"""Copy sealed native Cancel/default-tool preparation after exact source checks."""
from pathlib import Path
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]
stage=ROOT/'tools/move_ui_probe/cancel_delta'
frozen=json.loads((stage/'freeze.json').read_text(encoding='utf-8'))
for name,digest in frozen['hashes'].items():
    if hashlib.sha256((stage/name).read_bytes()).hexdigest()!=digest: raise RuntimeError('sealed tool source differs: '+name)
manifest=json.loads((stage/'production_copy_manifest.json').read_text(encoding='utf-8'))
prepared=[]
for entry in manifest['copy_sources']:
    target=ROOT/entry['target']; data=(ROOT/entry['source']).read_bytes()
    if hashlib.sha256(data).hexdigest()!=entry['sha256']: raise RuntimeError('tool candidate differs: '+entry['target'])
    if 'base_sha256' in entry:
        if hashlib.sha256(target.read_bytes()).hexdigest()!=entry['base_sha256']: raise RuntimeError('tool Root base differs: '+entry['target'])
    elif target.exists(): raise RuntimeError('new tool target exists: '+entry['target'])
    prepared.append((target,data))
for target,data in prepared:
    target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(data)
print('info: native Cancel/default preparation copied; actual native cleanup proof pending')
