"""Copy the frozen UI-only writer after the parent source review."""
import hashlib
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/hud_time/writer'
freeze = json.loads((source / 'evidence/freeze.json').read_text(encoding='utf-8'))
receipt_path = ROOT / 'artifacts/hud_writer_integration/copy_receipt.json'
if receipt_path.exists():
    raise RuntimeError('HUD writer copy receipt already exists')
if freeze['writer_reader_fixture_checks'] != 159 or freeze['process_fixture_checks'] != 10:
    raise RuntimeError('HUD writer freeze check counts differ')
records = {}
prepared = []
for entry in freeze['copy_manifest']:
    path = ROOT / entry['source']
    payload = path.read_bytes()
    if hashlib.sha256(payload).hexdigest() != entry['sha256']:
        raise RuntimeError('HUD writer freeze source differs: ' + str(path))
    destination = ROOT / 'src/game' / path.name
    if path.name == 'hud_time_reader.h':
        current = destination.read_bytes()
        expected = payload.replace(b'    friend class Writer;\r\n', b'').replace(b'    friend class Writer;\n', b'')
        if expected.replace(b'\r\n', b'\n') != current.replace(b'\r\n', b'\n'):
            raise RuntimeError('HUD reader header has changes beyond the private friend')
    if path.name == 'hud_time_writer_process.cpp':
        payload = payload.replace(b'#define WIN32_LEAN_AND_MEAN', b'#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif')
        payload = payload.replace(b'#define NOMINMAX', b'#ifndef NOMINMAX\n#define NOMINMAX\n#endif')
    if path.name != 'hud_time_reader.h' and destination.exists() and destination.read_bytes() != payload:
        raise RuntimeError('HUD writer existing destination differs: ' + str(destination))
    prepared.append((destination, payload))
    records[path.name] = {'frozen_sha256': entry['sha256'], 'production_sha256': hashlib.sha256(payload).hexdigest()}
for destination, payload in prepared:
    destination.write_bytes(payload)
receipt_path.parent.mkdir(parents=True, exist_ok=True)
receipt_path.write_text(json.dumps({'candidate': 'HUD-TIME-2-UI-ONLY', 'files': records,
    'native_writer_acceptance': False, 'production_delta': 'Windows macro guards only'}, indent=2) + '\n', encoding='utf-8')
print('info: reviewed HUD writer copied; receipt=' + str(receipt_path))
