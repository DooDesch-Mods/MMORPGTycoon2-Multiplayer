from pathlib import Path
import argparse
import hashlib

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.root / 'src/game/remove_diagnostics.cpp').read_text(encoding='utf-8')

def body(anchor, text=source):
    source = text
    start = source.index(anchor)
    end = (source.index(') {', start)+2 if anchor=='bool read_registered_building(' else source.index('{', start)) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

# Reverse only this split, so every existing guard and public consumer stays bound.
from scene_scenery_source_boundary import normalize
# Reverse only the explicit saved-Scenery read-only edges for the prior
# complete-source guard. Actual extraction below uses the current bodies.
metadata_delta = [('#include "dungeon_building_identity.inc"\n#include "root_scenery_metadata.inc"', '#include "dungeon_building_identity.inc"'), ('    if(!(readonly?read_metadata_prop(memory,reader,heap.image,before,actual,uid,prop,error):reader.prop(actual,uid,prop,error))||\n       !scenery_kiss::read_action_prop(uid,building,error))return false;', '    if(!reader.prop(actual,uid,prop,error)||!scenery_kiss::read_action_prop(uid,building,error))return false;'), ('                               const host::opened::CurrentWorld* admitted,bool metadata_only=false) {', '                               const host::opened::CurrentWorld* admitted) {'), ('    if(!(metadata_only?read_metadata_prop(memory,reader,heap.image,world,reinterpret_cast<void*>(object),uid,prop,error):\n         reader.prop(reinterpret_cast<void*>(object),uid,prop,error)))return false;', '    if(!reader.prop(reinterpret_cast<void*>(object),uid,prop,error))return false;'), ('        return resolve_local_target_impl(world.selection,uintptr_t(object),target,cause,&world.origin,true);', '        return resolve_local_target_impl(world.selection,uintptr_t(object),target,cause,&world.origin);')]
from saved_scenery_batch_source_boundary import normalize as batch_normalize, verify as batch_verify
batch_verify(args.root)
metadata_source=batch_normalize('src/game/remove_diagnostics.cpp',source)
for added,prior in metadata_delta:
    assert metadata_source.count(added)==1, 'Saved Scenery readonly boundary differs'
    metadata_source=metadata_source.replace(added,prior,1)
assert hashlib.sha256((args.root/'src/game/root_scenery_metadata.inc').read_bytes()).hexdigest()=='e38421273c129114d9981f0953a8a5c564739d70f9c8ce88a503e8c8e3079a2a', 'Saved Scenery readonly member helper differs'
original = normalize("src/game/remove_diagnostics.cpp", metadata_source, batch=False)
# Reverse only the exact D01 Building/Scene alias additions before the
# existing Modular and identity/refund comparisons. Every original guard
# and both historical complete-source hashes below remain unchanged.
dungeon_building_admission_delta = (
    ('#include "remove_diagnostics.h"\n#include "building_scene_identity.h"\n#include "character_boss_retirement_read.h"\n', '#include "remove_diagnostics.h"\n#include "character_boss_retirement_read.h"\n'),
    ('    std::string revocation_key;\n    bool operator==(const Bound&)const=default;\n};\n', '    std::string revocation_key;\n};\n'),
    ('namespace {\n#include "dungeon_building_identity.inc"\nbool read_registered_building(uint64_t uid,host::opened::CurrentWorld& opened,native::BuildingState& building,\n                              uintptr_t& object,uintptr_t& addresspoint,std::string& error,bool readonly=false,bool modular=false,const std::string& dungeon_scene={}) {\n    object=0;addresspoint=0;\n', 'namespace {\nbool read_registered_building(uint64_t uid,host::opened::CurrentWorld& opened,native::BuildingState& building,\n                              uintptr_t& object,uintptr_t& addresspoint,std::string& error,bool readonly=false,bool modular=false) {\n    object=0;addresspoint=0;\n'),
    ('    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& why){if(base>UINTPTR_MAX-offset){why="Modular identity address overflow";return false;}return memory.copy(base+offset,&value,sizeof value,why);};\n    if(!dungeon_scene.empty()){\n        if(modular||prop.class_addresspoint!=heap.image+0x16d93c0||\n           !dungeon_building_target(memory,heap.image,before.root,uintptr_t(actual),building,dungeon_scene,error))return false;\n    }else if(modular){if(!modular_identity::read(at,heap.image,uintptr_t(actual),building,error))return false;}\n    else if(!normal_building&&!normal_scenery){error=cause.empty()?"shared created target has no supported native family":cause;return false;}\n', '    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& why){if(base>UINTPTR_MAX-offset){why="Modular identity address overflow";return false;}return memory.copy(base+offset,&value,sizeof value,why);};\n    if(modular){if(!modular_identity::read(at,heap.image,uintptr_t(actual),building,error))return false;}\n    else if(!normal_building&&!normal_scenery){error=cause.empty()?"shared created target has no supported native family":cause;return false;}\n'),
    ('}\nstatic bool admit_created_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular,const std::string& dungeon_scene={}) {\n    error.clear();\n', '}\nstatic bool admit_created_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular) {\n    error.clear();\n'),
    ('    host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,addresspoint=0;\n    if(!read_registered_building(uid,opened,building,object,addresspoint,error,false,modular,dungeon_scene))return false;\n    const prop_lease::World world{opened.world_epoch,opened.generation,opened.world,opened.map};\n', '    host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,addresspoint=0;\n    if(!read_registered_building(uid,opened,building,object,addresspoint,error,false,modular))return false;\n    const prop_lease::World world{opened.world_epoch,opened.generation,opened.world,opened.map};\n'),
    ('bool admit_created_modular(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return admit_created_building_impl(snapshot,key,uid,error,true);}\nbool admit_created_dungeon_building(const std::string& snapshot,const std::string& key,uint64_t uid,const std::string& scene,std::string& error){\n    if(!action_session::valid_dungeon_building_scene(scene)){error="Dungeon Building admission scene invalid";return false;}\n    return admit_created_building_impl(snapshot,key,uid,error,false,scene);\n}\n\n', 'bool admit_created_modular(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return admit_created_building_impl(snapshot,key,uid,error,true);}\n\n'),
    ('}\nstatic bool restore_redone_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular,const std::string& dungeon_scene={}) {\n    if(!main_thread.load()||main_thread.load()!=GetCurrentThreadId()||snapshot!=net::current_snapshot_id()||\n', '}\nstatic bool restore_redone_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular) {\n    if(!main_thread.load()||main_thread.load()!=GetCurrentThreadId()||snapshot!=net::current_snapshot_id()||\n'),
    ('    host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,ap=0;\n    if(!read_registered_building(uid,opened,building,object,ap,error,false,modular,dungeon_scene))return false;\n    auto& saved=found->second;\n', '    host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,ap=0;\n    if(!read_registered_building(uid,opened,building,object,ap,error,false,modular))return false;\n    auto& saved=found->second;\n'),
    ('bool restore_redone_modular(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return restore_redone_building_impl(snapshot,key,uid,error,true);}\nbool restore_redone_dungeon_building(const std::string& snapshot,const std::string& key,uint64_t uid,const std::string& scene,std::string& error){\n    if(!action_session::valid_dungeon_building_scene(scene)){error="Dungeon Building admission scene invalid";return false;}\n    return restore_redone_building_impl(snapshot,key,uid,error,false,scene);\n}\n\n', 'bool restore_redone_modular(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return restore_redone_building_impl(snapshot,key,uid,error,true);}\n\n'),
    ('\nbool is_dungeon_building_target(const LocalTarget& target,const std::string& scene,std::string& error){\n    host::opened::CurrentWorld opened;native::BuildingState value;uintptr_t object=0,ap=0;\n    if(target.snapshot_id!=net::current_snapshot_id()||\n       !read_registered_building(target.uid,opened,value,object,ap,error,true,false,scene))return false;\n    if(object!=target.object||opened.world_epoch!=target.world_epoch||opened.generation!=target.world_generation||\n       value.type!=target.type||value.variant!=target.variant){\n        error="Dungeon Building exact current target identity differs";return false;\n    }\n    error.clear();return true;\n}\nbool dungeon_building_scene(const LocalTarget& target,std::string& scene,bool& matched,std::string& error){\n    scene.clear();matched=false;error.clear();\n    if(target.normal_building||target.normal_scenery)return true;\n    native::BuildingState value;\n    if(!scenery_kiss::read_action_prop(target.uid,value,error))return false;\n    if(value.type!="DungeonExterior"&&!value.dungeon_membership&&!value.dungeon_instance_membership)return true;\n    matched=true;\n    if(value.type=="DungeonExterior"&&!value.dungeon_membership&&!value.dungeon_instance_membership)scene="world";\n    else{\n        host::ProcessMemory memory;uintptr_t shard=0,region=0;\n        if(!memory.copy(target.object+0x170,&shard,8,error)||!memory.copy(target.object+0x178,&region,8,error))return false;\n        dungeon_scene_identity_runtime::World world;std::vector<dungeon_scene_roster::Scene> scenes;\n        if(!dungeon_scene_identity_runtime::read_scene_roster(world,scenes,error))return false;\n        for(const auto& row:scenes)if(row.owner.shard==shard&&row.owner.region==region&&row.owner.type==1){\n            if(!scene.empty()){error="Dungeon Building current scene owner is ambiguous";return false;}\n            scene=row.owner.key;\n        }\n        if(scene.empty()){error="Dungeon Building target has no current template owner";return false;}\n    }\n    if(!is_dungeon_building_target(target,scene,error)){scene.clear();return false;}\n    return true;\n}\nbool refresh_moved_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,const std::string& dungeon_scene) {\n    error.clear();host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,addresspoint=0;\n', '\nbool refresh_moved_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error) {\n    error.clear();host::opened::CurrentWorld opened;native::BuildingState building;uintptr_t object=0,addresspoint=0;\n'),
    ('    }\n    if(!read_registered_building(uid,opened,building,object,addresspoint,error,false,false,dungeon_scene))return false;\n    auto& saved=found->second;\n', '    }\n    if(!read_registered_building(uid,opened,building,object,addresspoint,error))return false;\n    auto& saved=found->second;\n'),
    ('\n#include "building_scene_identity.inc"\n}\n', '\n}\n'),
)
for dungeon_addition, prior_admission in dungeon_building_admission_delta:
    assert original.count(dungeon_addition) == 1, 'Dungeon Building admission boundary differs'
    original = original.replace(dungeon_addition, prior_admission, 1)
# Normalize only the exact independent Modular admission extension. Keep
# both historical whole-source guards bound to all existing consumers.
modular_admission_delta = (
    ('#include "modular_identity.h"\n', ''),
    ('                              uintptr_t& object,uintptr_t& addresspoint,std::string& error,bool readonly=false,bool modular=false) {\n', '                              uintptr_t& object,uintptr_t& addresspoint,std::string& error,bool readonly=false) {\n'),
    ('    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& why){if(base>UINTPTR_MAX-offset){why="Modular identity address overflow";return false;}return memory.copy(base+offset,&value,sizeof value,why);};\n    if(modular){if(!modular_identity::read(at,heap.image,uintptr_t(actual),building,error))return false;}\n    else if(!normal_building&&!normal_scenery){error=cause.empty()?"shared created target has no supported native family":cause;return false;}\n', '    if(!normal_building&&!normal_scenery){error=cause.empty()?"shared created target has no supported native family":cause;return false;}\n'),
    ('static bool admit_created_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular) {\n', 'bool admit_created_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error) {\n'),
    ('    if(!read_registered_building(uid,opened,building,object,addresspoint,error,false,modular))return false;\n', '    if(!read_registered_building(uid,opened,building,object,addresspoint,error))return false;\n'),
    ('bool admit_created_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return admit_created_building_impl(snapshot,key,uid,error,false);}\nbool admit_created_modular(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return admit_created_building_impl(snapshot,key,uid,error,true);}\n\n', ''),
    ('static bool restore_redone_building_impl(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error,bool modular) {\n', 'bool restore_redone_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error) {\n'),
    ('    if(!read_registered_building(uid,opened,building,object,ap,error,false,modular))return false;\n', '    if(!read_registered_building(uid,opened,building,object,ap,error))return false;\n'),
    ('bool restore_redone_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return restore_redone_building_impl(snapshot,key,uid,error,false);}\nbool restore_redone_modular(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){return restore_redone_building_impl(snapshot,key,uid,error,true);}\n\n', ''),
    ('bool is_modular_target(const LocalTarget& target,std::string& error){\n    host::opened::CurrentWorld opened;native::BuildingState value;uintptr_t object=0,ap=0;\n    if(target.snapshot_id!=net::current_snapshot_id()||!read_registered_building(target.uid,opened,value,object,ap,error,true,true))return false;\n    if(target.object!=object||target.type!=value.type||target.variant!=value.variant||target.world_epoch!=opened.world_epoch||target.world_generation!=opened.generation){error="Modular target full canonical World binding differs";return false;}error.clear();return true;\n}\n', ''),
)
for modular_addition, prior_admission in modular_admission_delta:
    assert original.count(modular_addition) == 1, "Modular admission boundary differs"
    original = original.replace(modular_addition, prior_admission, 1)
identity_wrapper = body('bool identity_bindings(') + '\n'
assert original.count(identity_wrapper) == 1
original = original.replace(identity_wrapper, '', 1)
# Character retirement is an independent include-only extension. Keep the
# original identity/refund guards bound to their exact prior source.
retirement_headers = ('#include "character_boss_retirement_read.h"\n'
                      '#include "typed_action/action_session.h"\n'
                      '#include "typed_action_runtime.h"\n'
                      '#include "move_contract.h"\n'
                      '#include "dungeon_scene_identity_runtime.h"\n'
                      '#include "region_saved_identity.h"\n')
retirement_runtime = '#include "character_boss_retirement_runtime.inc"\n\n'
assert original.count(retirement_headers) == 1, 'Character retirement include boundary differs'
assert original.count(retirement_runtime) == 1, 'Character retirement runtime boundary differs'
original = original.replace(retirement_headers, '', 1).replace(retirement_runtime, '', 1)
# Independent identity readers do not participate in this collector/refund split.
for anchor, suffix in (('bool resolve_current_building_key(', '\n\n'),
                       ('bool resolve_current_building_identity(', '\n'),
                       ('bool identity_targets(', '\n\n')):
    addition = body(anchor, original) + suffix
    assert original.count(addition) == 1, 'Scene identity lookup boundary differs'
    original = original.replace(addition, '', 1)
include = '#include "service_building_census.h"\n'
assert original.count(include) == 1, 'Service census include boundary differs'
original = original.replace(include, '', 1)
helper = ('namespace {\nbool resolve_local_target_impl(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& error,\n'
          '                               const host::opened::CurrentWorld* admitted) {')
assert original.count(helper) == 1
original = original.replace(helper, 'bool resolve_local_target(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& error) {', 1)
admission = ('    if(admitted) {\n'
             '        if(!host::same_world(*admitted,opened)){error="native UI admitted world changed before target observation";return false;}\n'
             '    } else {\n'
             '        json sources;if(!collect_bindings(sources,error,BindingDetails::Identity))return false;\n'
             '    }')
assert original.count(admission) == 1
original = original.replace(admission, '    json sources;if(!collect_bindings(sources,error,BindingDetails::Identity))return false;', 1)
local_wrapper = ('}\nbool resolve_local_target(uintptr_t manager,uintptr_t object,LocalTarget& out,std::string& error) {\n'
                 '    return resolve_local_target_impl(manager,object,out,error,nullptr);\n}\n')
assert original.count(local_wrapper) == 1
original = original.replace(local_wrapper, '', 1)
record_call = 'resolve_local_target_impl(world.selection,uintptr_t(object),out,error,&world.origin)'
assert original.count(record_call) == 1
original = original.replace(record_call, 'resolve_local_target(world.selection,uintptr_t(object),out,error)', 1)
assert hashlib.sha256(original.encode()).hexdigest() == '5929bfadd2acd723ad6cf07f199837bdaf52a4ffa9b1ea2fc1519de36010ff1c', 'Same-call admission changed another guard or consumer'
old = 'namespace {\nenum class BindingDetails {Identity,Refund};\nbool collect_bindings(json& out,std::string& error,BindingDetails details) {'
assert original.count(old) == 1
original = original.replace(old, 'bool bindings(json& out,std::string& error) {', 1)
for rows in ('retained', 'list'):
    new = f'if(details==BindingDetails::Refund&&!attach_refunds(world,{rows},error))'
    assert original.count(new) == 1
    original = original.replace(new, f'if(!attach_refunds(world,{rows},error))', 1)
wrapper = '}\nbool bindings(json& out,std::string& error) {\n    return collect_bindings(out,error,BindingDetails::Refund);\n}\n'
assert original.count(wrapper) == 1
original = original.replace(wrapper, '', 1)
edge = 'collect_bindings(sources,error,BindingDetails::Identity)'
assert original.count(edge) == 2
for anchor in ('bool resolve_local_target_impl(', 'bool resolve_record_target('):
    assert body(anchor).count(edge) == 1
original = original.replace(edge, 'bindings(sources,error)')
assert hashlib.sha256(original.encode()).hexdigest() == '6aa33a7d60b77f636002b1f0eb3086f71c7f1c94994559acf70b8a61a0d2fde2', 'Identity/refund split changed an unrelated guard or consumer'

pieces = [body('std::string binding_id('), body('std::string registry_id('),
          body('template<class Binding>bool import_bindings('),
          body('enum class BindingDetails ') + ';', body('bool collect_bindings_body('), body('bool collect_bindings('),
          body('bool bindings(json& out,'), body('bool identity_bindings('), body('bool attach_refunds(const host::opened::CurrentWorld& origin,'),
          body('bool resolve_local_target_impl('), body('bool resolve_local_target('), body('bool resolve_record_target(')]
pieces.append(body('bool identity_targets('))
args.output.mkdir(parents=True, exist_ok=True)
metadata=(args.root/'src/game/root_scenery_metadata.inc').read_text(encoding='utf-8')
(args.output/'root_metadata_prop_actual.inc').write_text(metadata,encoding='utf-8')
(args.output/'registered_metadata_actual.inc').write_text(body('bool read_registered_building(').replace('bool read_registered_building(', 'bool registered_metadata_actual(',1)+'\n',encoding='utf-8')
census=(args.root/'src/game/service_building_census.h').read_text(encoding='utf-8')
(args.output/'service_census_actual.inc').write_text(body('template<class Resolve>',census)+'\n',encoding='utf-8')
(args.output / 'identity_refund_split_type.inc').write_text(body('struct Bound ') + ';\n', encoding='utf-8')
(args.output / 'identity_refund_split_bodies.inc').write_text('\n'.join(pieces) + '\n', encoding='utf-8')
print('Extracted complete collector, public report, refund enrichment, both resolvers and registry import; full original source guard passed.')

# New owner-only consumers use the same actual known-Bound resolver.
owner=(args.root/'src/game/building_identity_census.inc').read_text(encoding='utf-8')
(args.output/'building_identity_census_actual.inc').write_text(owner,encoding='utf-8')
service=(args.root/'src/game/service_live_native.inc').read_text(encoding='utf-8')
callback=body('auto destination=',service)
assert callback.endswith('}')
(args.output/'service_destination_actual.inc').write_text(
    'bool service_destination_actual(const std::map<uintptr_t,std::string>& destinations,const std::set<uintptr_t>* proved_buildings,uintptr_t pointer,std::string& key,std::string& error){'+callback+';return destination(pointer,key,error);}',encoding='utf-8')
thought=(args.root/'src/game/toon_thought_frame.inc').read_text(encoding='utf-8')
assert thought.count('identity_building_targets(buildings,e)')==1
assert service.count('identity_building_targets(census,e,&local.service_destinations)')==1
assert service.count('service_sample(r,local.service_destinations,owner,value,e,&proved_buildings)')==1
assert service.count('service_sample(r,local.service_destinations,sampled,actual,cause)')==2, 'SaveScope destination samples must remain lookup-only'


# The Party callback remains inside the original SaveScope. Exercise its actual
# alias merge and lifetime checks without substituting an idle target resolver.
party=(args.root/'src/game/party_frame_native.cpp').read_text(encoding='utf-8')
assert 'interventions::actor_census(context_,binding_,census,error)' in body('    bool pins(',party)
assert 'if(!prop_pins(owners,keys,error)||!current(error))return false;' in body('    bool pins(',party)
assert '!checked(owner->second,error)' in body('    bool any_identify(',party)
assert '!checked(found->second,error)' in body('    bool any_resolve(',party)
assert 'resolve_identity_destination' not in party
(args.output/'party_owner_type_actual.inc').write_text(body('struct Owner ',party)+';\n',encoding='utf-8')
(args.output/'party_owner_methods_actual.inc').write_text(body('    bool checked(',party)+'\n'+body('    bool prop_pins(',party)+'\n',encoding='utf-8')

construction=(args.root/'src/game/developer_staff/construction_native.cpp').read_text(encoding='utf-8')
target=body('bool building(',construction)
assert 'resolve_identity_destination(context.snapshot_uuid,link.building_key,out.binding,e)' in target
assert 'resolve_record_target' not in target and 'resolve_local_target' not in target
(args.output/'construction_target_actual.inc').write_text(target+'\n',encoding='utf-8')

# Preserve the complete source census without copying its discarded binding array.
building_targets=body('bool identity_building_targets(',owner)
assert 'json selected=sources' not in building_targets
assert 'json selected={{"world_epoch",sources.at("world_epoch")},{"world_generation",sources.at("world_generation")},{"bindings",json::array()}};' in building_targets
assert building_targets.index('collect_bindings(sources,error,BindingDetails::BuildingTargets,&omitted)')<building_targets.index('json selected=')
assert building_targets.index('for(const auto& row:sources.at("bindings"))')<building_targets.index('service_building_census::read(selected,resolve,sampled,error)')
