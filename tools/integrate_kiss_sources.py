"""Copy the small reviewed coop adapters and retain exact source hashes."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
copies = {
    **{f'tools/placement_kiss/{name}': f'src/game/placement_kiss/{name}' for name in
       ('native_build.h', 'native_build.cpp', 'placement_adapter.h', 'placement_adapter.cpp', 'native_codeguards.h')},
    **{f'tools/multiplayer_world_kiss/{name}': f'src/game/multiplayer_world/{name}' for name in
       ('world_state.h', 'world_state.cpp', 'native_binding_adapter.h')},
}
prepared = {}
for source, target in copies.items():
    content = (ROOT / source).read_bytes()
    if target.endswith('/world_state.h'):
        content = content.replace(b'#include "action_session.h"', b'#include "../typed_action/action_session.h"')
    prepared[target] = (source, content)
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
record = ROOT / 'artifacts/placement_kiss' / stamp
record.mkdir(parents=True, exist_ok=False)
receipt = {'scope': 'source copy only; native acceptance remains pending', 'files': {}}
for target, (source, content) in prepared.items():
    path = ROOT / target
    if path.exists() and path.read_bytes() != content:
        raise RuntimeError('refusing changed production adapter: ' + target)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    receipt['files'][target] = {'source': source, 'source_sha256': hashlib.sha256((ROOT / source).read_bytes()).hexdigest(),
                              'production_sha256': hashlib.sha256(content).hexdigest()}
(record / 'copy.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
print('info: copied coop adapters receipt=' + str(record / 'copy.json'))
