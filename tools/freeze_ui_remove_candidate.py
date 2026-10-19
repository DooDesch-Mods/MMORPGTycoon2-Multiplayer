"""Seal current UI production sources before the final build and native proof."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
paths=[ROOT/p for p in ('src/game/ui_remove_runtime.cpp','src/game/ui_remove_runtime.h','src/game/typed_action_runtime.cpp',
    'src/game/typed_action_runtime.h','src/game/typed_action/action_session.h','src/game/typed_action/action_session.cpp',
    'src/game/remove_diagnostics.cpp','src/game/remove_diagnostics.h','src/hooks/hook_engine.cpp','src/core/dll_main.cpp',
    'tools/mcp/runtime.py','tools/mcp/server.py','tools/ui_remove_acceptance.py')]
paths+=list((ROOT/'src/game/ui_remove').rglob('*.h'))+list((ROOT/'src/game/ui_remove').rglob('*.cpp'))
out=ROOT/'artifacts/ui_remove_candidate'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out.mkdir(parents=True,exist_ok=False)
receipt={'candidate':'NATIVE-UI-REMOVE-1','sources':{p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)},
    'native_game_proven':False,'scope':'actual normal UI Remove capture with private typed authoritative native replay and genuine native BuildingInfoWindow MCP command',
    'prior_native_acceptance':'artifacts/acceptance/c9e11645-0bd8-4547-b691-05d68bab3711/typed_remove_checkpoint.json',
    'next_native_acceptance':'one Client UI input on exactly two small-world instances; no unchanged acceptance replay'}
(out/'freeze.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(f'info: UI candidate={out}/freeze.json sources={len(paths)}')
