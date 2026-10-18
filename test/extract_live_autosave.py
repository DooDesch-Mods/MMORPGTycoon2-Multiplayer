from pathlib import Path
import argparse

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

def body(path, anchor):
    s = (a.root / path).read_text(encoding='utf-8')
    start = s.index(anchor)
    end = s.index('{', start) + 1
    depth = 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]

for target, path, anchors in [
    ('autosave_activity.inc', 'src/game/live_host_state/native_port.h', ['inline bool native_activity(', 'inline bool publication_activity(']),
    ('autosave_scope.inc', 'src/game/live_host_state/native_port.cpp', ['bool current_scope(', 'bool NativePort::publication_ready(']),
    ('autosave_tutorial_publication.inc', 'src/game/live_host_state/bridge.cpp', ['bool same_tutorial_publication(']),
    ('autosave_tick.inc', 'src/game/live_host_state/bridge.cpp', ['bool Bridge::tick(']),
]:
    (a.output / target).write_text('\n'.join(body(path, anchor) for anchor in anchors) + '\n', encoding='utf-8')

native = body('src/game/live_host_state/native_port.cpp', 'bool once(')
assert 'current_scope(expected,binding,p,status,e)' in native
assert 'native_activity(status.saves_in_progress,status.edit_operations,expected_edits,e)' in native
assert 'publication_activity(' not in native
bridge = (a.root / 'src/game/live_host_state/bridge.cpp').read_text(encoding='utf-8')
for name in ['capture_before', 'publish_before', 'take', 'before_result']:
    assert 'publication_ready' not in body('src/game/live_host_state/bridge.cpp', 'bool Bridge::' + name + '(')
assert bridge.count('port_.publication_ready(') == 1
