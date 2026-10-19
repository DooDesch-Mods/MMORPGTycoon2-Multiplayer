"""Extract production admission guards; optionally verify a sealed source delta."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--overlay', type=Path)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--verify-delta', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parent
changes = json.loads((root / 'changes.json').read_text())
texts = {}
for row in changes:
    path = args.source / row['path']
    if args.overlay and (args.overlay / row['path']).is_file():
        path = args.overlay / row['path']
    raw = path.read_bytes()
    text = raw.decode('utf-8').replace('\r\n', '\n')
    texts[row['path']] = text
    if args.verify_delta:
        restored = text
        for change in reversed(row['changes']):
            assert restored.count(change['new']) == 1, row['path'] + ': changed block absent or ambiguous'
            restored = restored.replace(change['new'], change['old'])
        if b'\r\n' in raw:
            restored = restored.replace('\n', '\r\n')
        assert hashlib.sha256(restored.encode('utf-8')).hexdigest() == row['sha256'], row['path'] + ': unrelated source changed'

def block(path, index=0):
    value = next(row for row in changes if row['path'] == path)['changes'][index]['new']
    assert texts[path].count(value) == 1
    return value

native = block('src/game/party_live_native.cpp')
frame = block('src/game/party_frame_native.cpp')
actor_bind = block('src/game/intervention_actor_live.cpp', 2)
actor_current = block('src/game/intervention_actor_live.cpp', 1)
actor_root = '    if(state.root!=uintptr_t(current.map)+0x1c0)return fail(error,"Actor24 callback cached current Root differs from actual NativeOpen");'
assert actor_root in texts['src/game/intervention_actor_live.cpp']
actor_current += '\n' + actor_root
dungeon = block('src/game/dungeon_instance_native.cpp').split('\n    if(!census(')[0]
scene = block('src/game/scene_clone/runtime.cpp').split('\n    if(', 1)[1]
scene = '    if(' + scene.split('\n    if(!gizmo::read_current(')[0]

# The extracted bodies remain inside their existing Session/Controller,
# full-world, saved-identity, and unknown-outcome admission paths.
unchanged = {
    'src/game/intervention_actor_live.cpp': ['actual!=state.context', 'if(!routes(error)||!read_current(context,binding,values,locals,error)||!native_open_runtime::read_current_world(world,error))return false;', 'if(!install_cache(std::move(locals),error))return false;'],
    'src/game/dungeon_instance_native.cpp': ['if(!census(*next,next->owners,error))return life::fail(error,error.empty()?"binding completed native world differs":error);', 'bool fresh('],
    'src/game/scene_clone/runtime.cpp': ['!same_world(census.world,ready->baseline.world)', '!verify_direct(ready->baseline,direct::registry,e)', '!identities(ready->baseline,direct::registry,e)'],
    'src/game/party_live.cpp': ['bool Adapter::unknown(', 'locked_scope_'],
}
for path, needles in unchanged.items():
    for needle in needles:
        assert needle in texts[path], path + ': required path absent: ' + needle

generated = '''// Generated from exact production source by check.py.
namespace scope_cases {
bool party_native(const live::Raw& current,const typed::Context& context,const typed::Binding& binding,std::string& error){
''' + native + '''
    return true;
}
bool party_frame(const typed::Context& context,const typed::Context& context_,const typed::Binding& binding,const typed::Binding& binding_,const World& world,const live::Raw& prior_,std::string& error){
''' + frame + '''
}
bool actor_bind(ActorState& state,const typed::Context& context,const typed::Binding& binding,const World& world,uint64_t epoch,uint64_t origin,std::string& error){
''' + actor_bind + '''
    return true;
}
bool actor_current(const ActorState& state,const World& current,std::string& error){
''' + actor_current + '''
    return true;
}
bool dungeon_bind(const typed::Context& context,const typed::Binding& frame,const World& world,std::string& error){
''' + dungeon + '''
    return true;
}
bool scene_bind(const typed::Context& context,const typed::Binding& binding,const SceneState* ready,std::string& e){
''' + scene + '''
    return true;
}
}
'''
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'scope_bodies.inc').write_text(generated, encoding='utf-8')
(args.output / 'source-check.json').write_text(json.dumps({'status': 'PASS', 'source_files': len(changes), 'extracted_bodies': 6, 'whole_file_delta_verified': args.verify_delta}, indent=2) + '\n')
print('PASS: 6 production scope bodies extracted; sealed source delta verification=' + str(args.verify_delta))
