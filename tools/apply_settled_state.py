"""Install the source-bound two-file native height settlement delta."""
from pathlib import Path
import hashlib
import json
ROOT = Path(__file__).resolve().parents[1]
stage = ROOT / 'tools/placement_settled_state'
receipt = json.loads((stage / 'stage_receipt.json').read_text())
for name in ('world_state.h', 'world_state.cpp'):
    target = ROOT / 'src/game/multiplayer_world' / name
    source = stage / 'stage/multiplayer_world' / name
    assert hashlib.sha256(target.read_bytes()).hexdigest() == receipt['original_source_sha256']['src/game/multiplayer_world/' + name], 'production baseline changed'
    assert hashlib.sha256(source.read_bytes()).hexdigest() == receipt['staged_source_sha256']['stage/multiplayer_world/' + name], 'staged delta changed'
for name in ('world_state.h', 'world_state.cpp'):
    (ROOT / 'src/game/multiplayer_world' / name).write_bytes((stage / 'stage/multiplayer_world' / name).read_bytes())
print('info: native height settlement table installed')
