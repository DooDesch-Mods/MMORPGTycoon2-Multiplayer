"""Install the frozen second-witness delta after the root source review."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
freeze_path = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/hud_time/reader_witness/evidence/freeze.json'
freeze = json.loads(freeze_path.read_text(encoding='utf-8'))
receipt_path = ROOT / 'artifacts/hud_witness_integration/copy_receipt.json'
if receipt_path.exists():
    raise RuntimeError('HUD witness receipt already exists')
if freeze['fixture_checks'] != 180 or freeze['process_checks'] != 10:
    raise RuntimeError('HUD witness freeze check counts differ')
prepared = []
records = {}
for entry in freeze['copy_manifest']:
    source = ROOT / entry['source']
    payload = source.read_bytes()
    if hashlib.sha256(payload).hexdigest() != entry['sha256']:
        raise RuntimeError('frozen witness source hash differs: ' + str(source))
    destination = ROOT / 'src/game' / source.name
    current = destination.read_bytes()
    old_hash = hashlib.sha256(current).hexdigest()
    if source.name == 'hud_time_reader.h':
        anchor = b'    bool once(State&, Witness&, std::string&);'
        if current.count(anchor) != 1 or b'bool read_bound(' in current:
            raise RuntimeError('private reader method anchor differs')
        payload = current.replace(anchor, b'    bool read_bound(State&, Witness&, std::string&);\n' + anchor, 1)
    else:
        expected = {'hud_time_reader.cpp': '216e288ff2a8fa11cbcf08c7d41513b0f1eddcff215d14ad56e02ad1b4679609',
                    'hud_time_writer.cpp': 'e63b2964a0047e110e95f2666c56f670e780e5422c075362df9f265f69b9e74e'}[source.name]
        if old_hash != expected:
            raise RuntimeError('production baseline hash differs: ' + str(destination))
    prepared.append((destination, payload))
    records[source.name] = {'before_sha256': old_hash, 'frozen_sha256': entry['sha256'],
                           'production_sha256': hashlib.sha256(payload).hexdigest()}
for destination, payload in prepared:
    destination.write_bytes(payload)
    if hashlib.sha256(destination.read_bytes()).hexdigest() != records[destination.name]['production_sha256']:
        raise RuntimeError('production copy verification differs: ' + str(destination))
receipt_path.parent.mkdir(parents=True, exist_ok=True)
receipt_path.write_text(json.dumps({'candidate': 'HUD-TIME-3', 'files': records,
    'native_acceptance': False, 'header_merge': 'private read_bound declaration only'}, indent=2) + '\n', encoding='utf-8')
print('info: reviewed second-witness delta installed; receipt=' + str(receipt_path))
