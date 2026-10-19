"""Seal the Root bridge candidate for its targeted delegated regression receipt."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
paths=('src/game/typed_action_runtime.cpp','src/game/typed_action_runtime.h',
       'src/game/typed_action/action_session.h','src/game/typed_action/action_session.cpp',
       'src/game/ui_remove_runtime.h','src/game/ui_remove/ui_capture.h','src/game/ui_remove/ui_capture.cpp')
out=ROOT/'artifacts/typed_ui_candidate'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out.mkdir(parents=True,exist_ok=False)
receipt={'candidate':'TYPED-UI-ROOTBRIDGE-1','sources':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},
         'native_game_proven':False,'scope':'actual bound Controller context and private typed Engine callback replay authorization'}
(out/'freeze.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(f'info: typed UI bridge candidate={out}/freeze.json')
