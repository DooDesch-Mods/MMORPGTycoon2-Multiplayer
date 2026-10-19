"""Copy the reviewed frozen HUD reader without changing its native contracts."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/hud_time'
receipt = json.loads((source / 'evidence/result.json').read_text(encoding='utf-8'))
if receipt['passed'] is not True or receipt['compile_exit'] != 0:
    raise RuntimeError('HUD reader fixture build is not confirmed')
out = ROOT / 'artifacts/hud_reader_integration'
out.mkdir(parents=True, exist_ok=True)
record_path = out / 'copy_receipt.json'
if record_path.exists():
    raise RuntimeError('HUD reader copy receipt already exists')
copied = {}
for name in ('hud_time_reader.h', 'hud_time_reader.cpp', 'hud_time_reader_process.h', 'hud_time_reader_process.cpp'):
    payload = (source / name).read_bytes()
    expected = receipt['files'][name]
    if hashlib.sha256(payload).hexdigest() != expected:
        raise RuntimeError('frozen HUD source differs: ' + name)
    destination = ROOT / 'src/game' / name
    if destination.exists():
        raise RuntimeError('HUD destination already exists: ' + str(destination))
    if name == 'hud_time_reader_process.cpp':
        payload = payload.replace(b'#define WIN32_LEAN_AND_MEAN', b'#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif')
        payload = payload.replace(b'#define NOMINMAX', b'#ifndef NOMINMAX\n#define NOMINMAX\n#endif')
    destination.write_bytes(payload)
    copied[name] = {'frozen_sha256': expected, 'production_sha256': hashlib.sha256(destination.read_bytes()).hexdigest()}
record_path.write_text(json.dumps({'candidate': 'HUD-TIME-1-READONLY', 'files': copied,
    'production_delta': 'Windows macro guards only', 'native_runtime_proven': False}, indent=2) + '\n', encoding='utf-8')
print('info: reviewed HUD reader copied; receipt=' + str(record_path))
