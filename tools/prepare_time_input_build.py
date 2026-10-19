"""Prepare the immutable native input build and remove macro redefinition warnings."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
for name in ('shared_time.cpp', 'time_runtime_native.cpp', 'native_time_input.cpp', 'native_time_input_process.cpp'):
    path = ROOT / 'src/game' / name
    text = path.read_text(encoding='utf-8')
    for macro in ('WIN32_LEAN_AND_MEAN', 'NOMINMAX'):
        plain = f'#define {macro}\n'
        guarded = f'#ifndef {macro}\n#define {macro}\n#endif\n'
        if plain in text and guarded not in text:
            text = text.replace(plain, guarded)
    path.write_text(text, encoding='utf-8')
prior = json.loads((ROOT / 'artifacts/jobs/time_core_integration.json').read_text(encoding='utf-8'))
job = {'candidate': 'TIME-ACCEPTANCE-1-CORE', 'steps': prior['steps']}
path = ROOT / 'artifacts/jobs/time_input_integration.json'
if path.exists():
    raise RuntimeError('immutable input build job already exists')
path.write_text(json.dumps(job, indent=2) + '\n', encoding='utf-8')
print(f'info: input build job={path}; production macro guards set; staging remains frozen')
