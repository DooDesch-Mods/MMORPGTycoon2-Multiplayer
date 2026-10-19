"""Copy the reviewed frozen Calendar and transport without binding native authority."""
import hashlib
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
base = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/hud_time'
receipt_path = ROOT / 'artifacts/hud_calendar_integration/copy_receipt.json'
if receipt_path.exists():
    raise RuntimeError('Calendar copy receipt already exists')
prepared = []
records = {}
for name, count in (('calendar', 186), ('calendar_transport', 155)):
    freeze = json.loads((base / name / 'evidence/freeze.json').read_text(encoding='utf-8'))
    if freeze['fixture_checks'] != count:
        raise RuntimeError('Calendar fixture count differs: ' + name)
    for entry in freeze['copy_manifest']:
        source = ROOT / entry['source']
        payload = source.read_bytes()
        if hashlib.sha256(payload).hexdigest() != entry['sha256']:
            raise RuntimeError('frozen Calendar hash differs: ' + str(source))
        target = ROOT / 'src/game' / source.name
        if source.name == 'hud_time_reader.h':
            current = target.read_bytes()
            anchor = b'    friend class Writer;'
            expected = payload.replace(b'    friend class Calendar;\r\n', b'').replace(b'    friend class Calendar;\n', b'')
            baseline = current.replace(b'    bool read_bound(State&, Witness&, std::string&);\r\n', b'').replace(b'    bool read_bound(State&, Witness&, std::string&);\n', b'')
            if expected.replace(b'\r\n', b'\n') != baseline.replace(b'\r\n', b'\n') or current.count(anchor) != 1:
                raise RuntimeError('Calendar reader header has unrelated changes')
            payload = current.replace(anchor, anchor + b'\n    friend class Calendar;', 1)
        elif target.exists():
            raise RuntimeError('new Calendar source already exists: ' + str(target))
        if source.name == 'hud_calendar_process.cpp':
            payload = payload.replace(b'#define WIN32_LEAN_AND_MEAN', b'#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif')
        prepared.append((target, payload))
        records[source.name] = {'frozen_sha256': entry['sha256'], 'production_sha256': hashlib.sha256(payload).hexdigest()}
for target, payload in prepared:
    target.write_bytes(payload)
    if hashlib.sha256(target.read_bytes()).hexdigest() != records[target.name]['production_sha256']:
        raise RuntimeError('Calendar copy readback differs: ' + str(target))
receipt_path.parent.mkdir(parents=True, exist_ok=True)
receipt_path.write_text(json.dumps({'candidates': ['HUD-CALENDAR-1', 'HUD-CALENDAR-TRANSPORT-1'],
    'files': records, 'native_acceptance': False, 'delta': 'additive Calendar friend and Windows macro guards'}, indent=2) + '\n', encoding='utf-8')
print('info: reviewed Calendar sources copied; receipt=' + str(receipt_path))
