"""Copy the two delivered readonly catalog helpers after applying their CPP hunk."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'tools/scenery_gui/catalog_read_delta/manifest.json').read_text())
plan=[]
for source_name,row in manifest['new_files'].items():
    source=(ROOT/source_name).resolve();target=(ROOT/row['destination']).resolve()
    if not source.is_relative_to(ROOT/'tools/scenery_gui/catalog_read_delta/stage') or not target.is_relative_to(ROOT/'src/game/scenery_kiss') or target.exists():
        raise RuntimeError('catalog helper scope/existence differs: '+str(target))
    data=source.read_bytes()
    if hashlib.sha256(data).hexdigest()!=row['sha256']:raise RuntimeError('catalog helper source changed: '+str(source))
    plan.append((target,data))
for target,data in plan:
    target.write_bytes(data)
    if target.read_bytes()!=data:raise RuntimeError('catalog helper readback failed: '+str(target))
print('info: two readonly catalog helpers applied')
