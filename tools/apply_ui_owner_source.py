"""Install the sealed native-supported inactive UI ownership reader."""
from pathlib import Path
import hashlib
import json
ROOT = Path(__file__).resolve().parents[1]
stage = ROOT / 'tools/placement_ui_owner_delta/source_fix'
freeze = json.loads((stage / 'freeze.json').read_text())
for name, digest in freeze['hashes'].items():
    assert hashlib.sha256((stage / name).read_bytes()).hexdigest() == digest, name
manifest = json.loads((stage / 'production_copy_manifest.json').read_text())
for row in manifest['copy_sources']:
    source, target = ROOT / row['source'], ROOT / row['target']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == row['sha256']
    assert hashlib.sha256(target.read_bytes()).hexdigest() == row['base_sha256']
    target.write_bytes(source.read_bytes())
print('info: native-supported UI ownership reader copied')
