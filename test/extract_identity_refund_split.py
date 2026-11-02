from pathlib import Path
import argparse
import hashlib

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.root / 'src/game/remove_diagnostics.cpp').read_text(encoding='utf-8')

compact_collector_delta = [('bool collect_bindings_body(json& out,std::string& error,BindingDetails details,const std::set<uint64_t>* omit_metadata=nullptr) {', 'bool collect_bindings_body(json& out,std::string& error,BindingDetails details,const std::set<uint64_t>* omit_metadata=nullptr,const std::vector<uint64_t>* sorted_omissions=nullptr) {'), ('        retained_present=true;\n        if(!saved.revoked&&!saved.unknown&&(!omit_metadata||!omit_metadata->contains(saved.uid))) {', '        retained_present=true;\n        const bool omitted=sorted_omissions?std::binary_search(sorted_omissions->begin(),sorted_omissions->end(),saved.uid):\n            omit_metadata&&omit_metadata->contains(saved.uid);\n        if(!saved.revoked&&!saved.unknown&&!omitted) {'), ('        if(details==BindingDetails::BuildingTargets&&omit_metadata&&omit_metadata->contains(saved.uid)&&!saved.revoked&&!saved.unknown){', '        if(details==BindingDetails::BuildingTargets&&omitted&&!saved.revoked&&!saved.unknown){')]
compact_index_delta = [('template<class Index>\nbool building_identity_index(', 'template<class Index,class Omissions>\nbool building_identity_index('), ('                            std::set<uint64_t>& omitted,std::string& error){', '                            Omissions& omitted,std::string& error){'), ('    constexpr char scenery_suffix[]=":world:scenery:inline:";', '    if constexpr(requires{omitted.reserve(registry.size());})omitted.reserve(registry.size());\n    constexpr char scenery_suffix[]=":world:scenery:inline:";'), ('           binding.class_addresspoint==image+0x16c1390)omitted.insert(binding.uid);\n    }\n    return true;', '           binding.class_addresspoint==image+0x16c1390){\n            if constexpr(requires{omitted.push_back(binding.uid);})omitted.push_back(binding.uid);\n            else omitted.insert(binding.uid);\n        }\n    }\n    // The UID index already rejects duplicates. Sort compact exclusions once\n    // so membership and destination publication retain their canonical order.\n    if constexpr(requires{omitted.push_back(uint64_t{});})std::sort(omitted.begin(),omitted.end());\n    return true;')]
compact_targets_delta = [('std::pmr::unordered_map<uint64_t,const Bound*> indexed{&index_storage};indexed.reserve(registry.size());std::set<uint64_t> omitted;', 'std::pmr::unordered_map<uint64_t,const Bound*> indexed{&index_storage};indexed.reserve(registry.size());std::vector<uint64_t> omitted;'), ('collect_bindings_body(sources,error,BindingDetails::BuildingTargets,&omitted)', 'collect_bindings_body(sources,error,BindingDetails::BuildingTargets,nullptr,&omitted)'), ('!omitted.contains(row.at("local_uid").get<uint64_t>())', '!std::binary_search(omitted.begin(),omitted.end(),row.at("local_uid").get<uint64_t>())')]
retained_helper = '// A retained-only read needs uniqueness, not UID-to-Bound lookup. Keep this\n// proof local; any ineligible row uses the full index and its original errors.\nbool building_retained_scenery_index(const host::WorldState& world,uintptr_t image,\n                                     std::vector<uint64_t>& omitted){\n    omitted.clear();\n    if(unknown_locked||scene_scenery::saved_admission_pending())return false;\n    const prop_lease::World expected{world.origin.world_epoch,world.origin.generation,world.origin.world,world.origin.map};\n    if(admitted_world.epoch&&admitted_world!=expected)return false;\n    const auto snapshot=net::current_snapshot_id();const auto prefix=registry_id(snapshot,{});\n    std::vector<uintptr_t> objects;\n    constexpr char scenery_suffix[]=":world:scenery:inline:";\n    for(const auto& [id,binding]:registry){\n        if(binding.generation!=registry_generation||binding.snapshot!=snapshot||binding.scene_owner=="retired")continue;\n        // Revoked/Unknown rows can still appear in the collector\'s output.\n        if(binding.revoked||binding.unknown||binding.world!=expected||!binding.uid||!binding.entry||\n           binding.key.empty()||!id.starts_with(prefix)||id.compare(prefix.size(),std::string::npos,binding.key)!=0||\n           !binding.object||binding.scene_owner!="world"||!binding.scene_manager||!binding.scene_region||\n           binding.scene_shard!=world.root||!binding.scene_record.starts_with(binding.sha)||\n           binding.scene_record.compare(binding.sha.size(),sizeof(scenery_suffix)-1,scenery_suffix)!=0||\n           binding.class_addresspoint!=image+0x16c1390)return false;\n        if(omitted.empty()){omitted.reserve(registry.size());objects.reserve(registry.size());}\n        omitted.push_back(binding.uid);objects.push_back(binding.object);\n    }\n    if(omitted.empty())return false;\n    std::sort(omitted.begin(),omitted.end());std::sort(objects.begin(),objects.end());\n    return std::adjacent_find(omitted.begin(),omitted.end())==omitted.end()&&\n           std::adjacent_find(objects.begin(),objects.end())==objects.end();\n}\n'
compact_retained_delta = [('    std::pmr::unordered_map<uint64_t,const Bound*> indexed{&index_storage};indexed.reserve(registry.size());std::vector<uint64_t> omitted;\n    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;\n    const bool retained_scenery_only=!indexed.empty()&&indexed.size()==omitted.size()&&\n        !scene_scenery::saved_admission_pending();', '    std::pmr::unordered_map<uint64_t,const Bound*> indexed{&index_storage};std::vector<uint64_t> omitted;\n    const bool compact_retained=(!destinations||destinations_for_targets_only)&&\n        building_retained_scenery_index(world,heap.image,omitted);\n    if(!compact_retained){\n        indexed.reserve(registry.size());\n        if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;\n    }\n    const bool retained_scenery_only=compact_retained||(!indexed.empty()&&indexed.size()==omitted.size()&&\n        !scene_scenery::saved_admission_pending());'), ('    const auto prior_omitted=std::move(omitted);\n    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;', '    const auto prior_omitted=std::move(omitted);indexed.reserve(registry.size());\n    if(!building_identity_index(world,heap.image,indexed,omitted,error))return false;')]
def before_compact(text, changes):
    for old, new in changes:
        assert text.count(new)==1, 'Compact census production boundary changed'
        text=text.replace(new,old,1)
    return text

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
from ordered_identity_source_boundary import normalize as ordered_normalize
historical_source=ordered_normalize(source)
metadata_source=batch_normalize('src/game/remove_diagnostics.cpp',before_compact(historical_source,compact_collector_delta))
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
identity_wrapper = body('bool identity_bindings(', original) + '\n'
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
assert 'return resolve_record_target_impl(snapshot,key,out,error,false);' in body('bool resolve_record_target(')
for anchor in ('bool resolve_local_target_impl(', 'static bool resolve_record_target_impl('):
    assert body(anchor).count(edge) == 1
original = original.replace(edge, 'bindings(sources,error)')
assert hashlib.sha256(original.encode()).hexdigest() == '6aa33a7d60b77f636002b1f0eb3086f71c7f1c94994559acf70b8a61a0d2fde2', 'Identity/refund split changed an unrelated guard or consumer'

pieces = [body('std::string binding_id('), body('std::string registry_id('),
          body('template<class Binding>bool import_bindings('),
          body('enum class BindingDetails ') + ';', body('bool settle_current_identity('), body('bool collect_bindings_body('), body('bool collect_bindings('),
          body('bool bindings(json& out,'), body('bool identity_bindings('), body('bool attach_refunds(const host::opened::CurrentWorld& origin,'),
          body('bool resolve_local_target_impl('), body('bool resolve_local_target('), body('bool with_local_target_batch('), body('static bool resolve_record_target_impl('), body('bool resolve_record_targets('), body('bool resolve_record_target(')]
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
assert owner.count(retained_helper)==1, 'Retained compact proof boundary changed'
prior_compact_owner=before_compact(owner.replace(retained_helper,'',1),compact_retained_delta)
prior_compact_owner=before_compact(prior_compact_owner,compact_index_delta+compact_targets_delta)
index_body=body('bool building_identity_index(',prior_compact_owner)
object_index=('    using ObjectIndex=std::conditional_t<requires{indexed.bucket_count();},std::pmr::unordered_set<uintptr_t>,std::pmr::set<uintptr_t>>;\n'
              '    ObjectIndex objects{storage};\n'
              '    if constexpr(requires{objects.reserve(registry.size());})objects.reserve(registry.size());')
assert index_body.count(object_index)==1, 'Building object index container boundary changed'
index_body=index_body.replace(object_index,'    std::pmr::set<uintptr_t> objects{storage};',1)
(args.output/'building_identity_index_ordered_arena.inc').write_text(
    'template<class Index>\n'+index_body.replace('bool building_identity_index(', 'bool building_identity_index_ordered_arena(',1)+'\n',encoding='utf-8')
index_arena_delta=(
    ('Index& indexed', 'std::map<uint64_t,const Bound*>& indexed'),
    ('const auto prefix=registry_id(snapshot,{});\n'
     '    auto* storage=[&]{if constexpr(requires{indexed.get_allocator().resource();})return indexed.get_allocator().resource();else return std::pmr::get_default_resource();}();\n'
     '    std::pmr::set<uintptr_t> objects{storage};', 'const auto prefix=registry_id(snapshot,{});std::set<uintptr_t> objects;'),
)
for added,prior in index_arena_delta:
    assert index_body.count(added)==1, 'Building index local arena boundary changed'
    index_body=index_body.replace(added,prior,1)
pre_arena_index=index_body
(args.output/'building_identity_index_pre_arena.inc').write_text(
    pre_arena_index.replace('bool building_identity_index(', 'bool building_identity_index_pre_arena(',1)+'\n',encoding='utf-8')
# The measured baseline uses one upstream allocation per original tree node.
# Only allocator types change; all previous production guards remain extracted.
(args.output/'building_identity_index_measured.inc').write_text(
    pre_arena_index.replace('bool building_identity_index(', 'bool building_identity_index_measured(',1)
    .replace('std::map<uint64_t,const Bound*>& indexed','std::pmr::map<uint64_t,const Bound*>& indexed')
    .replace('std::set<uintptr_t> objects;','std::pmr::set<uintptr_t> objects;')+'\n',encoding='utf-8')
index_delta=(
    ('    const auto snapshot=net::current_snapshot_id();const auto prefix=registry_id(snapshot,{});std::set<uintptr_t> objects;\n'
     '    constexpr char scenery_suffix[]=":world:scenery:inline:";',
     '    const auto snapshot=net::current_snapshot_id();std::set<std::string> keys;std::set<uintptr_t> objects;'),
    ('        // Exact registry IDs are unique map keys. With one fixed prefix, their\n'
     '        // validated suffixes also prove canonical key uniqueness.\n'
     '        if(!binding.uid||!binding.entry||binding.key.empty()||!id.starts_with(prefix)||id.compare(prefix.size(),std::string::npos,binding.key)!=0||\n'
     '           !indexed.emplace(binding.uid,&binding).second||',
     '        if(!binding.uid||!binding.entry||binding.key.empty()||id!=registry_id(snapshot,binding.key)||\n'
     '           !keys.insert(binding.key).second||!indexed.emplace(binding.uid,&binding).second||'),
    ('           binding.scene_shard==world.root&&binding.scene_record.starts_with(binding.sha)&&\n'
     '           binding.scene_record.compare(binding.sha.size(),sizeof(scenery_suffix)-1,scenery_suffix)==0&&',
     '           binding.scene_shard==world.root&&binding.scene_record.starts_with(binding.sha+":world:scenery:inline:")&&'),
)
for added,prior in index_delta:
    assert index_body.count(added)==1, 'Building identity index allocation boundary changed'
    index_body=index_body.replace(added,prior,1)
assert hashlib.sha256(index_body.encode()).hexdigest()=='520714c51fc03269db8d7ed19a2b9ab159076ead6c4b96a4bc776403132a1ef3', 'Original Building identity index guard changed'
(args.output/'building_identity_index_baseline.inc').write_text(
    index_body.replace('bool building_identity_index(', 'bool building_identity_index_baseline(',1)+'\n',encoding='utf-8')
service=(args.root/'src/game/service_live_native.inc').read_text(encoding='utf-8')
owners=body('for(const auto& target:census)',service)
(args.output/'service_owners_actual.inc').write_text(owners+'\nreturn true;\n',encoding='utf-8')
callback=body('auto destination=',service)
assert callback.endswith('}')
(args.output/'service_destination_actual.inc').write_text(
    'bool service_destination_actual(const std::map<uintptr_t,std::string>& destinations,const std::set<uintptr_t>* proved_buildings,uintptr_t pointer,std::string& key,std::string& error){'+callback+';return destination(pointer,key,error);}',encoding='utf-8')
thought=(args.root/'src/game/toon_thought_frame.inc').read_text(encoding='utf-8')
assert thought.count('identity_building_targets(buildings,e)')==1
assert service.count('identity_building_targets(census,e,&local.service_destinations,true)')==1
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
building_targets=body('bool identity_building_targets(',prior_compact_owner)
scene=(args.root/'src/game/scenery_scene_identity.inc').read_text(encoding='utf-8')
(args.output/'saved_admission_pending_actual.inc').write_text(body('bool saved_admission_pending(',scene)+'\n',encoding='utf-8')
assert 'if(!saved_admission||saved_admission->consumed)return true;' in body('bool append_saved(',scene)
empty_retained_delta=('    const bool retained_scenery_only=!indexed.empty()&&indexed.size()==omitted.size()&&\n        !scene_scenery::saved_admission_pending();\n', '    // An empty retained census has no native target reads or pending alias\n    // admission. Its checked registry cannot change during collection.\n    if(retained_scenery_only&&sources.at("bindings").empty()&&(!destinations||destinations_for_targets_only))\n        return batch.finish(error);\n')
prior_double_index=building_targets
for added in empty_retained_delta:
    assert prior_double_index.count(added)==1, 'Retained empty census boundary changed'
    prior_double_index=prior_double_index.replace(added,'',1)
(args.output/'building_targets_double_index.inc').write_text(prior_double_index.replace('bool identity_building_targets(', 'bool identity_building_targets_double_index(',1)+'\n',encoding='utf-8')
# Retain the historical full collision index only in comparison baselines.
# The actual body still validates both registry scans and the complete census.
empty_collision_delta = (
    ('bool identity_building_targets(std::vector<LocalTarget>& out,std::string& error,std::map<uintptr_t,std::string>* destinations,bool destinations_for_targets_only){', 'bool identity_building_targets(std::vector<LocalTarget>& out,std::string& error,std::map<uintptr_t,std::string>* destinations){'),
    ('    // Empty Service censuses have no consumer for unrelated Scenery destinations.\n    if(destinations_for_targets_only&&sampled.empty())return batch.finish(error);\n', ''),
    ('    std::pmr::unordered_set<uintptr_t> candidate_objects{&index_storage};\n    if(!service_building_census::read(selected,resolve,sampled,error))return false;\n    if(!destinations&&!sampled.empty())candidate_objects.reserve(indexed.size());', '    std::pmr::unordered_set<uintptr_t> candidate_objects{&index_storage};if(!destinations)candidate_objects.reserve(indexed.size());\n    if(!service_building_census::read(selected,resolve,sampled,error))return false;'),
    ('    if(destinations||!sampled.empty())for(const auto uid:omitted){const auto* binding=indexed.at(uid);if(destinations)candidates.emplace(binding->object,binding->key);else candidate_objects.emplace(binding->object);}', '    for(const auto uid:omitted){const auto* binding=indexed.at(uid);if(destinations)candidates.emplace(binding->object,binding->key);else candidate_objects.emplace(binding->object);}'),
)
ordered_targets=prior_double_index
for added, prior in empty_collision_delta:
    assert ordered_targets.count(added)==1, 'Empty Building collision index boundary changed'
    ordered_targets=ordered_targets.replace(added,prior,1)
unordered_delta=(
    ('// Both fresh indexes share local storage;', '// Both fresh ordered indexes share local storage;'),
    ('std::pmr::unordered_map<uint64_t,const Bound*> indexed{&index_storage};indexed.reserve(registry.size());',
     'std::pmr::map<uint64_t,const Bound*> indexed{&index_storage};'),
    ('    std::pmr::unordered_set<uintptr_t> candidate_objects{&index_storage};if(!destinations)candidate_objects.reserve(indexed.size());\n', ''),
    ('    // The ordered exclusions preserve canonical insertion order. Thought callers\n'
     '    // only need pointer uniqueness, so their collision index stores no key strings.\n'
     '    for(const auto uid:omitted){const auto* binding=indexed.at(uid);if(destinations)candidates.emplace(binding->object,binding->key);else candidate_objects.emplace(binding->object);}',
     '    // Thought callers need the pointer collision proof, but not destination key copies.\n'
     '    for(const auto& [uid,binding]:indexed)if(omitted.contains(uid))candidates.emplace(binding->object,destinations?binding->key:std::string{});'),
    ('if(!(destinations?candidates.emplace(target.object,target.record_key).second:candidate_objects.emplace(target.object).second))',
     'if(!candidates.emplace(target.object,target.record_key).second)'),
)
for added,prior in unordered_delta:
    assert ordered_targets.count(added)==1, 'Building unordered index boundary changed'
    ordered_targets=ordered_targets.replace(added,prior,1)
(args.output/'building_targets_ordered_arena.inc').write_text(
    ordered_targets.replace('bool identity_building_targets(', 'bool identity_building_targets_ordered_arena(',1)
    .replace('building_identity_index(', 'building_identity_index_ordered_arena(')+'\n',encoding='utf-8')
arena_declaration=('    // Both fresh ordered indexes share local storage; no binding survives this call.\n'
                   '    std::pmr::monotonic_buffer_resource index_storage;\n'
                   '    std::pmr::map<uint64_t,const Bound*> indexed{&index_storage};std::set<uint64_t> omitted;')
assert ordered_targets.count(arena_declaration)==1
assert building_targets.count('const auto prior_omitted=std::move(omitted);')==1
pre_arena_targets=ordered_targets.replace(arena_declaration,'    std::map<uint64_t,const Bound*> indexed;std::set<uint64_t> omitted;',1)
pre_arena_targets=pre_arena_targets.replace('const auto prior_omitted=std::move(omitted);','const auto prior_omitted=omitted;',1)
(args.output/'building_targets_arena_baseline.inc').write_text(
    pre_arena_targets.replace('bool identity_building_targets(', 'bool identity_building_targets_arena_baseline(',1)
    .replace('building_identity_index(', 'building_identity_index_pre_arena(')+'\n',encoding='utf-8')
(args.output/'building_targets_arena_measured.inc').write_text(
    pre_arena_targets.replace('bool identity_building_targets(', 'bool identity_building_targets_arena_measured(',1)
    .replace('building_identity_index(', 'building_identity_index_measured(')
    .replace('std::map<uint64_t,const Bound*> indexed;', 'std::pmr::map<uint64_t,const Bound*> indexed;')+'\n',encoding='utf-8')
candidate_new=('    // Thought callers need the pointer collision proof, but not destination key copies.\n'
               '    for(const auto& [uid,binding]:indexed)if(omitted.contains(uid))candidates.emplace(binding->object,destinations?binding->key:std::string{});')
candidate_prior='    for(const auto& [uid,binding]:indexed)if(omitted.contains(uid))candidates.emplace(binding->object,binding->key);'
assert ordered_targets.count(candidate_new)==1, 'Omitted destination allocation boundary changed'
prior_targets=pre_arena_targets.replace(candidate_new,candidate_prior,1)
assert hashlib.sha256(prior_targets.replace('collect_bindings_body(sources,error,BindingDetails::BuildingTargets,&omitted)','collect_bindings(sources,error,BindingDetails::BuildingTargets,&omitted)',1).encode()).hexdigest()=='b1068edb1279cf130da6a2f2913c57acf1607e50d65fe441a775defd6780a416', 'Original Building target census body changed'
for name,actual,key in [('baseline',prior_targets,'binding->key'),('tracked',building_targets,'binding->key')]:
    if name=='baseline':actual=actual.replace('building_identity_index(', 'building_identity_index_pre_arena(')
    # Count bytes supplied to the omitted candidate map while executing the
    # complete extracted body. No census or native-reader predicate is replaced.
    statement='candidates.emplace(binding->object,'+key+');'
    assert actual.count(statement)==1
    actual=actual.replace(statement,'candidates.emplace(binding->object,candidate_key_probe('+key+'));',1)
    if name=='tracked':
        statement='candidate_objects.emplace(binding->object);'
        assert actual.count(statement)==1
        actual=actual.replace(statement,'(candidate_key_probe(std::string{}),candidate_objects.emplace(binding->object));',1)
    actual=actual.replace('bool identity_building_targets(', 'bool identity_building_targets_'+name+'(',1)
    (args.output/('building_targets_'+name+'.inc')).write_text(actual+'\n',encoding='utf-8')
assert building_targets.count('building_identity_index(world,heap.image,indexed,omitted,error)')==2, 'Both fresh Building identity scans must remain'
assert 'json selected=sources' not in building_targets
assert 'json selected={{"world_epoch",sources.at("world_epoch")},{"world_generation",sources.at("world_generation")},{"bindings",json::array()}};' in building_targets
assert building_targets.index('collect_bindings_body(sources,error,BindingDetails::BuildingTargets,&omitted)')<building_targets.index('json selected=')
assert building_targets.index('for(const auto& row:sources.at("bindings"))')<building_targets.index('service_building_census::read(selected,resolve,sampled,error)')

assert building_targets.count('collect_bindings_body(sources,error,BindingDetails::BuildingTargets,&omitted)')==1
assert 'collect_bindings(sources,error,BindingDetails::BuildingTargets,&omitted)' not in building_targets
assert building_targets.count('batch.finish(error)')==3, 'Empty and nonempty publication must retain final proof'

# Count complete actual index invocations without replacing their guards.
for name,actual in [('single',building_targets),('double',prior_double_index)]:
    actual=actual.replace('bool identity_building_targets(', 'bool identity_building_targets_'+name+'_measured(',1)
    actual=actual.replace('!building_identity_index(world,heap.image,indexed,omitted,error)', '!(++identity_index_calls,building_identity_index(world,heap.image,indexed,omitted,error))')
    (args.output/('building_targets_'+name+'_measured.inc')).write_text(actual+'\n',encoding='utf-8')

# Compare the complete immediate prior collector, index and census against the
# current extracted production body, rather than a mock algorithm.
prior_collector=body('bool collect_bindings_body(',before_compact(source,compact_collector_delta))
prior_index=body('bool building_identity_index(',prior_compact_owner)
prior_targets=body('bool identity_building_targets(',prior_compact_owner)
prior_collector=prior_collector.replace('bool collect_bindings_body(', 'bool collect_bindings_body_before_compact(',1)
prior_index=prior_index.replace('bool building_identity_index(', 'bool building_identity_index_before_compact(',1)
prior_targets=prior_targets.replace('bool identity_building_targets(', 'bool identity_building_targets_before_compact(',1).replace('building_identity_index(', 'building_identity_index_before_compact(').replace('collect_bindings_body(', 'collect_bindings_body_before_compact(')
(args.output/'compact_census_baseline.inc').write_text(prior_collector+'\n'+'template<class Index>\n'+prior_index+'\n'+prior_targets+'\n',encoding='utf-8')
