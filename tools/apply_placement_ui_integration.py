"""Apply the sealed normal native UI integration with exact source guards."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re
ROOT = Path(__file__).resolve().parents[1]
stage = ROOT / 'tools/placement_ui_integration'
freeze = json.loads((stage / 'freeze.json').read_text())
manifest = json.loads((stage / 'production_copy_manifest.json').read_text())
assert freeze['passed'] is True
assert hashlib.sha256((stage / 'production_copy_manifest.json').read_bytes()).hexdigest() == freeze['copy_manifest_sha256']
for name, digest in freeze['hashes'].items():
    assert hashlib.sha256((stage / name).read_bytes()).hexdigest() == digest, name
for name, digest in manifest['root_baselines'].items():
    assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, 'Root baseline changed: ' + name
header = (stage / 'tree/src/game/placement_ui_probe/codeguards.h').read_text()
assert '0x593cc0,' not in header, 'existing hooked Update must not be guarded as an unpatched entry'
for row in manifest['copy_sources']:
    source, target = ROOT / row['source'], ROOT / row['target']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == row['sha256'], row['source']
    if 'base_sha256' in row:
        assert hashlib.sha256(target.read_bytes()).hexdigest() == row['base_sha256'], row['target']
    else:
        assert not target.exists(), 'new target already exists: ' + row['target']
retained = ROOT / 'artifacts/placement_ui_integration' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
retained.mkdir(parents=True, exist_ok=False)
for row in manifest['copy_sources']:
    source, target = ROOT / row['source'], ROOT / row['target']
    if target.exists():
        backup = retained / row['target']; backup.parent.mkdir(parents=True, exist_ok=True); backup.write_bytes(target.read_bytes())
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(source.read_bytes())
(retained / 'copy.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('info: normal UI integration copied, receipt=' + str(retained / 'copy.json'))
