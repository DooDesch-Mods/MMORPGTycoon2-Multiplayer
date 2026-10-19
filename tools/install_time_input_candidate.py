"""Install the frozen input adapter without replacing the shared time runtime."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance'
HASHES = {
    'native_time_input.h': 'ae8298ddcaa937f120ca129ad1ef69cb19666e875af0361f8517a7058c1b60ca',
    'native_time_input.cpp': '2ca54d23491437646c21220cee2cb323e28b4d3a77a87daf589c78bef66be505',
    'native_time_input_process.cpp': 'b104cf6fd6ad4fe93ea19441fbca1a1e74234bff5d4942a6a10057d877b3cd00',
}
receipt = ROOT / 'artifacts/time_input_integration/copy_receipt.json'
if receipt.exists():
    raise RuntimeError('immutable input installation receipt already exists')
for name, expected in HASHES.items():
    if hashlib.sha256((SOURCE / name).read_bytes()).hexdigest() != expected:
        raise RuntimeError(f'frozen input source differs: {name}')
    if (ROOT / 'src/game' / name).exists():
        raise RuntimeError(f'input destination already exists: {name}')
receipt.parent.mkdir(parents=True, exist_ok=True)
for name in HASHES:
    (ROOT / 'src/game' / name).write_bytes((SOURCE / name).read_bytes())
receipt.write_text(json.dumps({'candidate': 'TIME-ACCEPTANCE-1', 'files': HASHES}, indent=2) + '\n', encoding='utf-8')
print(f'info: copied {len(HASHES)} frozen input files; receipt={receipt}')
