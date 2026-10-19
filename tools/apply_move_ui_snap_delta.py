"""Merge the sealed snap delta without replacing building lifecycle changes."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
stage = ROOT / 'tools/move_ui_snap_delta'
freeze = json.loads((stage / 'freeze.json').read_text(encoding='utf-8'))
for name, digest in freeze['hashes'].items():
    if hashlib.sha256((stage / name).read_bytes()).hexdigest() != digest:
        raise RuntimeError('sealed snap source changed: ' + name)
target = ROOT / 'src/game/move_ui_probe/runtime.cpp'
original = target.read_text(encoding='utf-8')
if 'normal_building' not in original:
    raise RuntimeError('building lifecycle admission is absent')
content = original
lines = (stage / 'root.patch').read_text(encoding='utf-8').splitlines(keepends=True)
index = 0
while index < len(lines):
    if not lines[index].startswith('@@ '):
        index += 1
        continue
    index += 1
    before, after = [], []
    while index < len(lines) and not lines[index].startswith(('@@ ', '--- a/')):
        line = lines[index]
        index += 1
        if line.startswith((' ', '-')):
            before.append(line[1:])
        if line.startswith((' ', '+')):
            after.append(line[1:])
    old, new = ''.join(before), ''.join(after)
    if content.count(old) != 1:
        raise RuntimeError('snap hunk context differs')
    content = content.replace(old, new, 1)
if 'normal_building' not in content or 'final_intent_pose' not in content:
    raise RuntimeError('required lifecycle or pose admission lost')
receipt = {'candidate': freeze['candidate'], 'before_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
target.write_bytes(content.encode('utf-8'))
receipt['after_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
receipt['state'] = 'integrated-unbuilt'
out = ROOT / 'artifacts/move_ui_snap_root'
out.mkdir(parents=True, exist_ok=True)
(out / 'integration.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
print(json.dumps(receipt))
