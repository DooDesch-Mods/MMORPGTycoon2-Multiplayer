"""Merge sealed normal Move diagnostics after the reviewed Undo interop."""
from pathlib import Path
import hashlib
import json
ROOT = Path(__file__).resolve().parents[1]
stage = ROOT / 'tools/move_ui_probe'
frozen = json.loads((stage / 'freeze.json').read_text(encoding='utf-8'))
for name,digest in frozen['hashes'].items():
    if hashlib.sha256((stage/name).read_bytes()).hexdigest() != digest: raise RuntimeError('sealed UI source changed: ' + name)
manifest = json.loads((stage / 'production_copy_manifest.json').read_text(encoding='utf-8'))
prepared = []
for entry in manifest['copy_sources']:
    target = ROOT / entry['target']; source = ROOT / entry['source']; data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != entry['sha256']: raise RuntimeError('UI source changed: ' + entry['target'])
    if entry['target'] == 'src/game/ui_move_runtime.cpp': continue
    if 'base_sha256' in entry:
        if hashlib.sha256(target.read_bytes()).hexdigest() != entry['base_sha256']: raise RuntimeError('UI target changed: ' + entry['target'])
    elif target.exists(): raise RuntimeError('new UI target exists: ' + entry['target'])
    prepared.append((target,data))
target = ROOT / 'src/game/ui_move_runtime.cpp'
if hashlib.sha256(target.read_bytes()).hexdigest() != 'd1cc77371953b2062fc5032ba2bd490cdfd471fb64946e30d83e1bce85912070': raise RuntimeError('reviewed interop base differs')
content = target.read_text(encoding='utf-8'); lines = (stage / 'root.patch').read_text(encoding='utf-8').splitlines(keepends=True)
index = 0
while index < len(lines):
    if lines[index].startswith('--- a/') and not lines[index].startswith('--- a/src/game/ui_move_runtime.cpp'): break
    if not lines[index].startswith('@@ '): index += 1; continue
    index += 1; before=[]; after=[]
    while index < len(lines) and not lines[index].startswith(('@@ ','--- a/')):
        line=lines[index]; index+=1
        if line.startswith((' ','-')): before.append(line[1:])
        if line.startswith((' ','+')): after.append(line[1:])
    old,new=''.join(before),''.join(after)
    if content.count(old) != 1: raise RuntimeError('UI hunk context differs')
    content=content.replace(old,new,1)
if 'move_kiss::Calls current_calls;' not in content: raise RuntimeError('Undo owner recheck lost')
prepared.append((target,content.encode('utf-8')))
for target,data in prepared:
    target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(data)
print('info: sealed normal Move diagnostic merged; Undo owner recheck retained')
