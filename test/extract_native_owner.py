from pathlib import Path
import argparse

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)


def body(text, anchor):
    start = text.index(anchor)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def write(name, text):
    (a.output / name).write_text(text + '\n', encoding='utf-8')


source = (a.root / 'src/game/live_host_state/native_port.cpp').read_text(encoding='utf-8')
once = body(source, 'bool once(')
owner = body(source, 'bool owner_once(')
current = body(source, 'bool NativePort::current_owners(')
assert current.count('!owner_once(') == 1
assert 'read_simulation_state' in once and 'read_simulation_state' not in owner
assert 'once(context_,binding_,first,pause1,e)' in body(source, 'bool NativePort::read(')
assert 'once(context_,binding_,second,pause2,e)' in body(source, 'bool NativePort::read(')
assert 'current_owners(pin,e)' in body(source, 'bool NativePort::store(')
baseline = current.replace('NativePort::current_owners(', 'NativePort::baseline(', 1).replace('!owner_once(', '!once(', 1)
write('owner_bodies.inc', once + '\n' + owner + '\n' + baseline + '\n' + current)
header = (a.root / 'src/game/live_host_state/state.h').read_text(encoding='utf-8')
write('owner_types.inc', body(header, 'struct Values {') + ';\n' + body(header, 'struct Pin {') + ';')
native_header = (a.root / 'include/mt2mp/game/native_engine.h').read_text(encoding='utf-8')
native_source = (a.root / 'src/game/native_engine.cpp').read_text(encoding='utf-8')
write('native_types.inc', body(native_header, 'struct ClockState {') + ';\n' + body(native_header, 'struct SimulationState {') + ';')
write('native_bodies.inc', '\n'.join(body(native_source, name) for name in ('bool in_game()', 'bool read_clock(', 'bool read_simulation_state(')))
ledger = (a.root / 'src/game/live_host_state/state.cpp').read_text(encoding='utf-8')
write('owner_ledger.inc', body(ledger, 'action_session::Ledger ledger('))
activity = (a.root / 'src/game/live_host_state/native_port.h').read_text(encoding='utf-8')
write('owner_activity.inc', body(activity, 'inline bool native_activity('))
