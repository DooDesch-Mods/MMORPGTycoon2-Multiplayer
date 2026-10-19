"""Freeze the HUD time fix and exact native MCP implementation before building."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
paths=['src/game/hud_time_reader.h','src/game/hud_time_reader.cpp','src/game/hud_time_activation.h','src/game/hud_time_activation.cpp',
    'src/game/hud_calendar_transport.cpp','src/game/hud_calendar_runtime.cpp','src/game/time_mouse_diagnostics.h','src/game/time_mouse_diagnostics.cpp',
    'src/game/time_native_button/native_button.h','src/game/time_native_button/native_button.cpp','src/game/time_native_button/button_original.h',
    'src/game/native_engine.cpp','include/mt2mp/game/native_engine.h','src/core/dll_main.cpp','tools/mcp/server.py','tools/mcp/runtime.py','CMakeLists.txt']
out=ROOT/'artifacts/time_mouse_candidate'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out.mkdir(parents=True,exist_ok=False)
receipt={'candidate':'HUD-MOUSE-TIME-1','native_proven':False,'files':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in paths}}
(out/'freeze.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('info: time mouse candidate freeze='+str(out/'freeze.json'))
