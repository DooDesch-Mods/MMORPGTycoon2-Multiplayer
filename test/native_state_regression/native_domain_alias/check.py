"""Extract actual UUID-domain consumer guards; optional sealed-delta check."""
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
args.output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parent
changes = json.loads((root / 'changes.json').read_text())
texts = {}
for row in changes:
    path = args.source / row['path']
    if args.overlay and (args.overlay / row['path']).is_file():
        path = args.overlay / row['path']
    raw = path.read_bytes()
    text = raw.decode().replace('\r\n', '\n')
    texts[row['path'].removeprefix('src/game/')] = text
    for change in row['changes']:
        assert text.count(change['new']) == change['count'], row['path'] + ': selected production block changed'
    if args.verify_delta:
        restored = text
        for change in reversed(row['changes']):
            restored = restored.replace(change['new'], change['old'])
        if b'\r\n' in raw:
            restored = restored.replace('\n', '\r\n')
        assert hashlib.sha256(restored.encode()).hexdigest() == row['sha256'], row['path'] + ': sealed delta differs'

def between(name, start, stop):
    text = texts[name]
    assert text.count(start) == 1, (name, start)
    begin = text.index(start)
    end = text.index(stop, begin)
    return text[begin:end]

def change(name, index=0):
    return next(row for row in changes if row['path'] == 'src/game/' + name)['changes'][index]['new']

prop = between('intervention_census.cpp', '    if(props.at("world_epoch")', '    std::vector<npc_probe::action_runtime::StateOwner> npcs;')
assert prop.endswith('    }\n'), 'Actor-only prop scope closing boundary differs'
prop=prop[:-6]
actor_source=texts['intervention_census.cpp']
actor_start=actor_source.index('    if(world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation)')
actor_end=actor_source.index('    std::vector<npc_probe::action_runtime::StateOwner> npcs;',actor_start)
actor_scope=actor_source[actor_start:actor_end]
actor_old=actor_scope[actor_scope.index('    nlohmann::json props;'):-6]
restored=actor_source[:actor_start]+actor_old+actor_source[actor_end:]
wrappers='bool census(const action_session::Context& context,const action_session::Binding& binding,Census& out,std::string& error){\n    return census_impl(context,binding,out,error,true);\n}\nbool actor_census(const action_session::Context& context,const action_session::Binding& binding,Census& out,std::string& error){\n    return census_impl(context,binding,out,error,false);\n}\n'
assert restored.count(wrappers)==1
restored=restored.replace(wrappers,'',1).replace('static bool census_impl(const action_session::Context& context,const action_session::Binding& binding,Census& out,std::string& error,bool include_props){','bool census(const action_session::Context& context,const action_session::Binding& binding,Census& out,std::string& error){',1)
actor_order = next(change for row in changes if row['path']=='src/game/intervention_census.cpp' for change in row['changes'] if 'std::pair{uint8_t(a.target.family)' in change['old'])
assert actor_source.count(actor_order['new'])==1, 'Actual actor comparator absent or ambiguous'
restored=restored.replace(actor_order['new'],actor_order['old'],1)
(args.output/'actor_order_comparators.inc').write_text('inline constexpr auto previous_actor_less='+actor_order['old']+';\ninline constexpr auto actual_actor_less='+actor_order['new']+';\n',encoding='utf-8')
assert hashlib.sha256(restored.encode()).hexdigest()=='c57f27a03719083f8aa271162bb6243fa1e375fcd655405bab6dbbe5a40faaaf', 'Actor-only census changed another roster, native read, failure or final publication guard'
actor_runtime=(args.source/'src/game/intervention_actor_runtime.cpp').read_text(encoding='utf-8')
assert actor_runtime.count('interventions::actor_census(')==2
assert hashlib.sha256(actor_runtime.replace('interventions::actor_census(','interventions::census(').encode()).hexdigest()=='d00723cf3b6ffdff06fd9513d1efa044a5dfe8a0070bf86d78a56a7e4d91685b', 'Actor reader changed beyond its two census entrypoints'
actor_body='namespace cases::actor_only {\nbool add(Census& out,uintptr_t pointer,const std::string& key,std::string& error){return ::fixture_add(out,pointer,key,error); }\nbool props(bool include_props,const typed::Context& context,const typed::Binding& binding,const World& world,Census& next,std::string& error){const uintptr_t image=0;\n'+actor_scope+'    return true;\n}\n}\n'
(args.output/'actor_census_scope.inc').write_text(actor_body,encoding='utf-8')
release = between('release_reference_runtime.inc', '    for(const auto& prop:props){', '    std::vector<npc_probe::action_runtime::StateOwner> npcs;')
character = between('character_membership_sample.inc', '    if(scenes.world.map!=out.world.origin.map', '    for(const auto& pool:scenes.pools)')
clone = between('scene_clone/quest_preview.inc', 'bool clone_current(', 'bool clone_same(')
same = between('npc_quest_target_runtime.cpp', 'template<class Left,class Right>bool same_world(', 'void reset(')
destination = between('npc_quest_target_runtime.cpp', '        if(current_scenes.world.uuid!=actual.uuid', '        if(!validate_scene(scene,error))')
cascade = between('dungeon_scene_identity_runtime.cpp', '    const bool stroke=request.action==action_session::Action::SetDungeonPalette', '    Census actual;if(!current_census(')
restoration = between('dungeon_scene_identity_runtime.cpp', '    if((request.action!=action_session::Action::UndoBuilding&&request.action!=action_session::Action::RedoBuilding)', '    identity::Registry next;identity::Scope expected=')
graph = between('dungeon_cells_graph.inc', '    std::map<uintptr_t,std::string> owners;if(!external_owners(', '    for(const auto& pool:before.scenes.pools)')
exterior = change('dungeon_scene_identity_runtime.cpp', 2)
party_scope = change('character_membership_sample.inc', 1).split('append({', 1)[1].split('}', 1)[0]
monster_scope = change('character_membership_sample.inc', 2).split('enumerate(', 1)[1].split(',out.monster_aliases', 1)[0]
saved_origin = change('scene_clone/quest_preview.inc', 1)

required = {
    'intervention_census.cpp': ['!boss_current::read_with_characters(context,binding,next.bosses,next.definitions,next.users,error)', 'if(prop.at("revoked").get<bool>())continue;', 'lookup(image,prop.at("local_uid").get<uint64_t>(),found)', '!same(world,after)'],
    'dungeon_instance_runtime.cpp': ['current!=expected', '!typed_action_runtime::read_binding(expected.snapshot_uuid,actual,error)||actual!=binding', '!same_world(ready.world,after)', 'Parties parties{memory,image,{ready.world.uuid'],
    'release_reference_runtime.inc': ['if(!context(r,c,b,pin,e))return false;', 'remove_diagnostics::identity_targets(props,e)', 'current!=c', 'final.uuid!=world.uuid'],
    'character_membership_sample.inc': ['prop.at("snapshot_id")!=snapshot', '!action_runtime::bound(out.world,binding,snapshot,error)', 'context.snapshot_uuid!=snapshot'],
    'dungeon_scene_identity_runtime.cpp': ['if(scope(copy_world(world))!=after)', 'scope(before.world)!=scope(actual.world)', '!registry.verify_roster(after,members,error)', '!controller.value("pending",false)||controller.value("locked",true)'],
    'scene_clone/quest_preview.inc': ['if(!validate_scene(fresh,error)', '!clone_same(scene_pools,preview.state->before)', 'scene_pools=std::move(preview.state->after)'],
    'ui_build_runtime.cpp': ['!host::same_world(world,pending.world)||shard!=pending.shard', 'pending.error', 'original false-branch cleanup or children differs; no replay'],
}
for name, needles in required.items():
    for needle in needles:
        assert needle in texts[name], name + ': preserved proof absent: ' + needle

# The composed consumer receives the first complete Character admission from
# Boss. Its final independent admission and all canonical membership guards stay
# mandatory; direct-call spelling in Intervention no longer describes this path.
boss_path = args.source / 'src/game/boss_current.cpp'
if args.overlay and (args.overlay / 'src/game/boss_current.cpp').is_file():
    boss_path = args.overlay / 'src/game/boss_current.cpp'
boss = boss_path.read_text(encoding='utf-8')

def boss_body(anchor):
    assert boss.count(anchor) == 1, 'Boss composed admission boundary differs: ' + anchor
    begin = boss.index(anchor)
    end = boss.index('{', begin) + 1
    depth = 1
    while depth:
        depth += (boss[end] == '{') - (boss[end] == '}')
        end += 1
    return boss[begin:end]

first = boss_body('bool admitted_characters(')
assert first[first.index('{'):] == '{\n    return npc_probe::character_actor_records::read_current(context,binding,definitions,users,error)&&native_open_runtime::read_current_world(world,error);\n}', 'Boss first complete Character/world admission differs'
final = boss_body('bool admitted(')
assert final[final.index('{'):] == '{\n    character_definition_identity::Census definitions;character_membership_reader::Users users;\n    return admitted_characters(context,binding,world,definitions,users,error);\n}', 'Boss final admission must obtain its own fresh Character census'
reader = boss_body('static bool read_impl(')
reader_required = [
    'out={};State next;if(!admitted_characters(context,binding,next.world,definitions,users,error)||!pools(next.world,next.pools,error))return false;',
    'if(!boss_identity::registry.enumerate(scope,entries,error))return false;',
    'if(!expected.emplace(row.pointer,std::pair{key,row}).second)return fail(error,"Boss live canonical registry repeats a current native object");',
    'if(!objects.insert(member.pointer).second||!ids.insert(member.uid).second||found==expected.end()||found->second.second.scope!=pool.scope||found->second.second.uid!=member.uid)return fail(error,"Boss current complete scene member lacks its exact confirmed canonical full UID");',
    'if(next.live.size()!=expected.size())return fail(error,"Boss confirmed canonical binding absent from complete actual scene census");',
    'if(!admitted(context,binding,after,error)||!same(next.world,after))return fail(error,error.empty()?"Boss current bound NativeOpen changed before readback":error);out=std::move(next);error.clear();return true;',
]
positions = []
for needle in reader_required:
    assert reader.count(needle) == 1, 'Boss composed owner/full UID guard absent: ' + needle
    positions.append(reader.index(needle))
assert positions == sorted(positions), 'Boss composed admission/final readback ordering differs'
publish = boss_body('bool read_with_characters(')
assert publish[publish.index('{'):] == '{\n    definitions={};users={};character_definition_identity::Census next_definitions;character_membership_reader::Users next_users;\n    if(!read_impl(context,binding,out,next_definitions,next_users,error))return false;\n    definitions=std::move(next_definitions);users=std::move(next_users);return true;\n}', 'Boss composed Character output must publish only after final admission'

generated = '''// Generated from selected production source by check.py.
namespace cases {
namespace props_detail {
bool add(Census& out,uintptr_t pointer,const std::string& key,std::string& error){return ::fixture_add(out,pointer,key,error);}
bool props_read(const json& props,const World& world,const typed::Context& context,const typed::Binding& binding,Census& next,std::string& error){const uintptr_t image=0;
''' + prop + '''    return true;
}
}
using props_detail::props_read;
bool release(const std::vector<Target>& props,const World& world,const typed::Context& c,const typed::Binding& b,Census& next,std::string& e){
    const auto add=[&](uintptr_t pointer,const char*,const std::string& key){return ::fixture_add(next,pointer,key,e);};
''' + release + '''    return true;
}
bool instance(const typed::Context& expected,const typed::Binding& binding,const World& world,std::string& error){
''' + change('dungeon_instance_runtime.cpp') + '''
    return true;
}
bool character(const SceneCensus& scenes,const Sample& out,const typed::Binding& binding,std::string& error){
''' + character + '''    return true;
}
Scope party_scope(const Sample& out,const typed::Binding& binding){return {''' + party_scope + '''};}
Scope monster_scope(const Sample& out,const typed::Binding& binding){return {''' + monster_scope + '''};}
namespace quest {
''' + same + clone + '''
bool destination(const SceneCensus& current_scenes,const World& actual,const ScenePool& scene,std::string& error){
    const auto fresh=current_scenes.pools.begin();
''' + destination + '''    return true;
}
q::Origin saved_origin(const Raw& raw){Fresh fresh;
    ''' + saved_origin + ''';
    return fresh.saved.origin;
}
}
bool cascade(const typed::Binding& binding,const typed::Request& request,const Raw& before,const json& controller,std::string& error){
''' + cascade + '''    return true;
}
bool restoration(const typed::Binding& binding,const typed::Request& request,const Raw& before,const json& controller,std::string& error){
''' + restoration + '''    return true;
}
bool exterior(uintptr_t building,uint64_t uid,Exterior& row,std::string& error){const auto snapshot=net::current_snapshot_id();
    return ''' + exterior + ''';
}
bool graph(Memory& memory,const GraphSample& before,std::string& error){
''' + graph + '''    return true;
}
bool purchase(const Raw& fresh,std::string& error){
''' + change('ui_build_runtime.cpp') + '''
    return true;
}
}
'''
(args.output / 'domain_bodies.inc').write_text(generated, encoding='utf-8')
(args.output / 'source-check.json').write_text(json.dumps({'status': 'PASS', 'production_files': 10, 'selected_files': len(changes), 'verify_delta': args.verify_delta}, indent=2) + '\n')
print('PASS: 9 domain deltas, composed Boss Character admission, existing destination fixture and preserved owner/failure paths; sealed delta=' + str(args.verify_delta))
