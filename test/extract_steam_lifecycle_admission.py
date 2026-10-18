from pathlib import Path
import argparse
import hashlib
import json

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

capture = (a.root / 'src/game/subscriber_lifecycle/runtime_capture.cpp').read_text()
append = (a.root / 'src/game/subscriber_lifecycle/runtime_append.cpp').read_text()
client = (a.root / 'src/game/subscriber_lifecycle/client_world.cpp').read_text()
monetary = (a.root / 'src/game/live_host_state/day_rollover/economy/native_monetary.cpp').read_text()

# Complete current production reader helpers, including both census passes.
helpers = capture[capture.index('bool fail('):capture.index('bool uuid(')]
helpers += capture[capture.index('struct Member {'):capture.index('bool bytes(')]
(a.output / 'steam_capture_readers.inc').write_text(helpers)

read_world = body(append, 'bool read_world(')
assert 'w.frame?&w.frame->context:nullptr' in read_world
read_roots = body(append, 'bool read_world_roots(')
assert 'current_frame(w,e)&&stage::read_client_world_roots' in read_roots
assert 'w.frame?&w.frame->context:nullptr' in read_roots
(a.output / 'steam_append_readers.inc').write_text(body(append, 'bool fail(') + '\n' + body(append, 'bool current_frame(') + '\n' + read_world + '\n' + read_roots)
batch = (a.root / 'src/game/subscriber_lifecycle/quest_edit_batch.inc').read_text()
(a.output / 'steam_held_verifier.inc').write_text(body(batch, 'struct QuestEditBatch {') + ';\n' + body(batch, 'bool quest_edits_held(') + '\n' + body(batch, 'bool verify_quest_edits_held('))
assert 'read_world_roots(batch.world,true,current,e)' in body(batch, 'bool verify_quest_edits_held(')
assert batch.count('read_world_roots(') == 1
assert batch.count('read_world(') == 5
for anchor in ['bool quest_fresh(', 'bool progress_fresh(']:
    assert 'read_world(w.world,true,current,e)' in body(append, anchor)


private = body(capture, 'bool capture(const std::filesystem::path&')
shared = body(capture, 'bool capture_frame(')
assert 'cause,nullptr,nullptr)' in private
assert 'cause,&graph,&expected)' in shared
(a.output / 'steam_capture_entries.inc').write_text(private + '\n' + shared)

implementation = body(capture, 'bool capture_impl(')
for line in implementation.splitlines():
    if 'read_pin(' in line or '!pin(' in line:
        assert '!wire, expected)' in line, 'Every capture owner check must retain production admission'
assert implementation.count('!wire, expected)') == 5
assert 'capture_frame(id,w.uuid,p.context,graph,receipt,e)' in monetary
assert 'current_frame(w,e)&&stage::read_client_world' in read_world
for anchor in ['bool read_client_world(', 'bool read_client_world_roots(']:
    reader = body(client, anchor)
    assert reader.count('root_once(memory,sha,origin,held,') == 2
    assert reader.count(',e,expected)') == 2
assert body(client, 'bool read_client_world(').count('read_cohort(') == 2
assert 'read_cohort(' not in body(client, 'bool read_client_world_roots(')

# Both unbound private callers still select the default no-Context restriction.
assert 'return apply_impl(graph,origin,nullptr,result,e);' in append
assert 'work.frame=frame;' in implementation or 'work.frame=frame;' in append
sources = {
    'runtime_capture.cpp': capture,
    'runtime_append.cpp': append,
    'client_world.cpp': client,
    'quest_edit_batch.inc': batch,
    'native_monetary.cpp': monetary,
}
(a.output / 'steam_lifecycle_sources.json').write_text(json.dumps(
    {name: hashlib.sha256(text.encode()).hexdigest() for name, text in sources.items()}, indent=2) + '\n')
print('Current production Host/Client admission bodies and all Frame/private call sites extracted')
