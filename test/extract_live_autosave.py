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
    ('autosave_owners.inc', 'src/game/live_host_state/native_port.cpp', ['bool NativePort::current_owners(', 'bool NativePort::store(']),
    ('autosave_tutorial_publication.inc', 'src/game/live_host_state/bridge.cpp', ['bool same_tutorial_publication(']),
    ('autosave_tick.inc', 'src/game/live_host_state/bridge.cpp', ['bool Bridge::tick(']),
]:
    actual='\n'.join(body(path, anchor) for anchor in anchors) + '\n'
    if target=='autosave_tick.inc':actual=actual.replace('bool Bridge::tick(', 'bool Bridge::tick_actual(', 1)
    (a.output / target).write_text(actual, encoding='utf-8')

native = body('src/game/live_host_state/native_port.cpp', 'bool once(')
assert 'current_scope(expected,binding,p,status,e)' in native
assert 'native_activity(status.saves_in_progress,status.edit_operations,expected_edits,e)' in native
assert 'publication_activity(' not in native
bridge = (a.root / 'src/game/live_host_state/bridge.cpp').read_text(encoding='utf-8')
for name in ['capture_before', 'publish_before', 'take', 'before_result']:
    assert 'publication_ready' not in body('src/game/live_host_state/bridge.cpp', 'bool Bridge::' + name + '(')
assert bridge.count('port_.publication_ready(') == 1

header=(a.root/'src/game/live_host_state/bridge.h').read_text(encoding='utf-8')
assert 'std::function<std::chrono::steady_clock::time_point()> capture_clock={}' in header
tick=body('src/game/live_host_state/bridge.cpp','bool Bridge::tick(')
assert tick.index('if(!capture(action,frame,e))return false;')<tick.index('last_capture_=capture_clock_?capture_clock_():std::chrono::steady_clock::now();')
assert 'now-*last_capture_<std::chrono::milliseconds(250)' in tick
assert 'GetTickCount' not in tick
assert tick.index('port_.publication_ready(')<tick.index('if(paused_idle_equal_&&')<tick.index('port_.read(current,e)')<tick.index('if(!capture(action,frame,e))')
assert 'now-*last_capture_<std::chrono::milliseconds(1000)' in tick
assert 'current.values==last_->values' in tick
assert 'last_->for_action.empty()' in tick
for name in ['capture_before', 'suspend_snapshot', 'suspend_lobby', 'rebind_snapshot']:
    assert 'paused_idle_equal_=false;' in body('src/game/live_host_state/bridge.cpp','bool Bridge::'+name+'(')
before=body('src/game/live_host_state/bridge.cpp','bool Bridge::capture_before(')
assert before.index('paused_idle_equal_=false;')<before.index('capture(previous,frame,e)')
