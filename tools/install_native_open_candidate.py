"""Install the reviewed NativeOpen source with immutable hash receipts."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'tools/engine_analysis/object_identity/build_capture'
HASHES = {
    'native_open_witness.h': '15fb6dc0ed5ee7c6d0728ab629a13924c1816ea3112c3b06a826181483984fcb',
    'native_open_witness.cpp': '9e81f8960c860434d74e6be5a04b341e5dae5885e1996410fe031dfbc506450d',
}
receipt = ROOT / 'artifacts/native_open_integration/copy_receipt.json'
if receipt.exists():
    raise RuntimeError('NativeOpen installation receipt is immutable')
for name, digest in HASHES.items():
    if hashlib.sha256((SOURCE / name).read_bytes()).hexdigest() != digest:
        raise RuntimeError(f'NativeOpen frozen source differs: {name}')
    if (ROOT / 'src/game' / name).exists():
        raise RuntimeError(f'NativeOpen destination already exists: {name}')
for name in HASHES:
    payload = (SOURCE / name).read_bytes()
    if name.endswith('.cpp'):
        payload = payload.replace(b'#define WIN32_LEAN_AND_MEAN', b'#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif')
        payload = payload.replace(b'#define NOMINMAX', b'#ifndef NOMINMAX\n#define NOMINMAX\n#endif')
    (ROOT / 'src/game' / name).write_bytes(payload)
receipt.parent.mkdir(parents=True, exist_ok=True)
receipt.write_text(json.dumps({'candidate': 'NATIVE-OPEN-1', 'frozen_files': HASHES,
                              'production_files': {name: hashlib.sha256((ROOT / 'src/game' / name).read_bytes()).hexdigest()
                                                   for name in HASHES},
                              'delta': 'production macro guards only'}, indent=2) + '\n', encoding='utf-8')
print(f'info: NativeOpen copied files={len(HASHES)} receipt={receipt}; hooks not installed')
