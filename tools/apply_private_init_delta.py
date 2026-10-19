"""Apply the sealed native constructor delta and preserve replaced sources."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[1]
delta = ROOT / 'tools/placement_ui_probe/private_init_delta'
freeze = json.loads((delta / 'freeze.json').read_text())
manifest = json.loads((delta / 'production_copy_manifest.json').read_text())
for name, digest in freeze['hashes'].items():
    assert hashlib.sha256((delta / name).read_bytes()).hexdigest() == digest, name
header = (delta / 'tree/native_codeguards.h').read_text()
assert '{0x592450,code_592450,sizeof(code_592450)}' in header
match = re.search(r'code_592450\[\]\s*=\s*\{([^}]+)\}', header)
assert match, 'native filter guard missing'
guard = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]{2})', match[1]))
assert guard == (delta / 'evidence/592450.bin').read_bytes(), 'native filter body differs'
target = ROOT / 'artifacts/placement_init_delta' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
target.mkdir(parents=True, exist_ok=False)
for row in manifest['sources']:
    source, destination = ROOT / row['source'], ROOT / row['target']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == row['sha256']
    (target / destination.name).write_bytes(destination.read_bytes())
    destination.write_bytes(source.read_bytes())
(target / 'copy.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('info: native init delta applied, retained=' + str(target))
