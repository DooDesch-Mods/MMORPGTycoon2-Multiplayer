"""Install the reviewed key-specific readiness delta with previous-source guards."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance'
HASHES = {
    'native_time_input.h': ('ae8298ddcaa937f120ca129ad1ef69cb19666e875af0361f8517a7058c1b60ca', 'eb79abf29e6cd2a4e6c11500fa79a654f227a6164ced2f76e68f0de4651a69c9'),
    'native_time_input.cpp': ('2ca54d23491437646c21220cee2cb323e28b4d3a77a87daf589c78bef66be505', '50d905b6f3a35181341477cbd36f1b40b253cab740eb02463edb9a89a6069d39'),
}
receipt = ROOT / 'artifacts/time_input_integration/readiness/copy_receipt.json'
if receipt.exists():
    raise RuntimeError('immutable readiness installation receipt already exists')
planned = []
for name, (previous, frozen) in HASHES.items():
    source = (SOURCE / name).read_bytes()
    path = ROOT / 'src/game' / name
    before = path.read_bytes()
    normalized = before.decode('utf-8').replace('\r\n', '\n')
    for macro in ('WIN32_LEAN_AND_MEAN', 'NOMINMAX'):
        normalized = normalized.replace(f'#ifndef {macro}\n#define {macro}\n#endif\n', f'#define {macro}\n')
    previous_hashes = {hashlib.sha256(payload).hexdigest() for payload in
                       (normalized.encode('utf-8'), normalized.replace('\n', '\r\n').encode('utf-8'))}
    if hashlib.sha256(source).hexdigest() != frozen or previous not in previous_hashes:
        raise RuntimeError(f'readiness source/destination guard failed: {name}; source={hashlib.sha256(source).hexdigest()}; destination normalized={sorted(previous_hashes)}')
    after = source.decode('utf-8')
    for macro in ('WIN32_LEAN_AND_MEAN', 'NOMINMAX'):
        after = after.replace(f'#define {macro}\n', f'#ifndef {macro}\n#define {macro}\n#endif\n')
    planned.append((name, path, before, after.encode('utf-8')))
receipt.parent.mkdir(parents=True, exist_ok=False)
for name, path, before, after in planned:
    (receipt.parent / f'{name}.before').write_bytes(before)
    path.write_bytes(after)
receipt.write_text(json.dumps({'files': {name: {'frozen_sha256': HASHES[name][1], 'production_sha256': hashlib.sha256(after).hexdigest()} for name, _, _, after in planned}}, indent=2) + '\n', encoding='utf-8')
prior = json.loads((ROOT / 'artifacts/jobs/time_input_integration.json').read_text(encoding='utf-8'))
job = ROOT / 'artifacts/jobs/time_input_readiness.json'
if job.exists():
    raise RuntimeError('immutable readiness build job already exists')
job.write_text(json.dumps({'candidate': 'TIME-INPUT-READINESS-CORE', 'steps': prior['steps']}, indent=2) + '\n', encoding='utf-8')
print(f'info: readiness delta copied; receipt={receipt}; build job={job}')
