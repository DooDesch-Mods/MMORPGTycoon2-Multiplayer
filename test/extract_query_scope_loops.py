from pathlib import Path
import argparse,hashlib

p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
text=(a.root/'src/game/live_host_state/day_rollover/economy/monetary.cpp').read_text()
def body(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]

# Extract the production extent declaration, not a test-only approximation.
census_extents=[
    ('src/game/intervention_census.cpp','bool census(const action_session::Context&'),
    ('src/game/boss_current.cpp','bool pools(const World&'),
    ('src/game/character_actor_records_runtime.inc','bool read_current(const action_session::Context&'),
    ('src/game/dungeon_scene_identity_runtime.cpp','static bool read_census('),
    ('src/game/dungeon_scene_identity_runtime.cpp','static bool current_exteriors('),
    ('src/game/dungeon_scene_identity_runtime.cpp','bool read_scene_roster('),
    ('src/game/release_reference_runtime.inc','bool reference_keys(Runtime&'),
]
declaration='live_host_state::apply_timing::PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());'
for relative,anchor in census_extents:
    selected=body((a.root/relative).read_text(encoding='utf-8'),anchor)
    first=selected[selected.index('{')+1:].lstrip()
    expected=declaration.replace('live_host_state::','live::') if 'release_reference' in relative else declaration
    assert first.startswith(expected), 'Complete census extent must begin before its first native read: '+relative
(a.output/'census_reader_extent.inc').write_text(declaration+'\n',encoding='utf-8')
for relative,call in [
    ('src/game/intervention_census.cpp','remove_diagnostics::identity_bindings(props,error)'),
    ('src/game/intervention_census.cpp','npc_quest_target_runtime::bind_current_destination(zone.value.pointer,key,dungeon,error)'),
    ('src/game/release_reference_runtime.inc','general_project_runtime::release_reference_keys(c,b,out,e)'),
    ('src/game/release_reference_runtime.inc','remove_diagnostics::identity_targets(props,e)'),
]:
    prefix='live::apply_timing::' if 'release_reference' in relative else 'live_host_state::apply_timing::'
    boundary='([&]{'+prefix+'PageReadScope original_read('+prefix+'PageBoundary::OriginalCall);return '+call+';}())'
    assert (a.root/relative).read_text(encoding='utf-8').count(boundary)==1, 'Mixed reader boundary must retain original-call invalidation: '+call

selected=body(text,'static Receipt apply_existing_body(')
actual_selected=selected
selected=selected.replace('        const auto quest_rows=[&](std::string& error){\n','',1)
selected=selected.replace('if(!port.store_quest_state(expected,i,old,after,error))return false;', 'if(!port.store_quest_state(expected,i,old,after,receipt.cause)){stop("actual Toon Quest state Unknown");return receipt;}',1)
selected=selected.replace('        return true;\n        };\n        if(!port.with_quest_stores(expected,target,quest_rows,receipt.cause)){stop("actual Toon Quest state Unknown");return receipt;}','',1)
original=selected
quest_phase_lines=[
    '        if(!port.begin_quest_stores(expected,receipt.cause)){stop("actual Toon Quest phase admission Unknown");return receipt;}\n',
    '        struct QuestScope {Port& port;~QuestScope(){port.end_quest_stores();}} quest_scope{port};\n',
]
for line in quest_phase_lines:
    assert selected.count(line)==1, 'Quest phase lifetime boundary changed'
    original=original.replace(line,'',1)
for part,name in [('QuestStores','quest'),('NpcStores','npc')]:
    anchor='{apply_timing::Scope measured(apply_timing::Part::'+part+');'
    assert selected.count(anchor)==1
    wrapped=body(selected,anchor)
    loop=wrapped[len(anchor)+1:-1]
    # Removing only the diagnostic opening/closing lines recreates the old body.
    assert loop.endswith('        ')
    historical_wrapped=wrapped
    historical_loop=loop
    for line in quest_phase_lines:
        historical_wrapped=historical_wrapped.replace(line,'',1)
        historical_loop=historical_loop.replace(line,'',1)
    original=original.replace(historical_wrapped,historical_loop[8:-8].rstrip('\n'),1)
    (a.output/(name+'_store_loop.inc')).write_text(body(actual_selected,anchor)+'\n')
expected='5089327e297507c4045d417a88a4b75163365e667564f768e8bef637f2d759f7'
assert hashlib.sha256(original.encode()).hexdigest()==expected, 'Quest/NPC diagnostic wrappers changed original Apply body'

def prior_child_reader(value):
    guard='if (saved.pool.records.empty() && !building_child_detail::has_region(id)) continue;'
    edge='        if (!building_child_detail::append(id, region, census, lookup, rows, error)) return false;\n'
    assert value.count(guard)==1 and value.count(edge)==1, 'Building child NPC saved-reader boundary changed'
    return value.replace(guard,'if (saved.pool.records.empty()) continue;',1).replace(edge,'',1)

# Only the two hash-bound, read-only NPC FindUID calls omit metadata invalidation.
find_scope_guards=[{'path': 'src/game/npc_remove_runtime.inc', 'anchor': 'bool state_owners_impl(', 'old': '{live_host_state::apply_timing::PageReadScope original_lookup(live_host_state::apply_timing::PageBoundary::OriginalCall);seh=lookup(api.find,op.local_uid,registered);}', 'new': 'seh=lookup(api.find,op.local_uid,registered);', 'hash': '2eac164a369ef3932a373eee7457275b82c24716f65252fe49cbdfefb5352421'}, {'path': 'src/game/npc_saved_identity_runtime.cpp', 'anchor': 'bool read_current(std::vector<Current>& out, std::string& error, bool share_metadata)', 'old': '{live_host_state::apply_timing::PageReadScope original_lookup(live_host_state::apply_timing::PageBoundary::OriginalCall);code = find_call(api.find, uid, pointer);}', 'new': 'code = find_call(api.find, uid, pointer);', 'hash': 'e814327eb7f53bd89bc42e3c84929937a971e0f6600097610a7c96ea3bd55d3d'}]
for item in find_scope_guards:
    source=(a.root/item['path']).read_text(encoding='utf-8')
    actual=body(source,item['anchor'])
    if item['path'].endswith('npc_saved_identity_runtime.cpp'):actual=prior_child_reader(actual)
    assert actual.count(item['new'])==1 and item['old'] not in actual
    original=actual.replace(item['new'],item['old'],1)
    if item['path'].endswith('npc_remove_runtime.inc'):
        assert original.count('npc_saved_identity_runtime::read_current(saved,error,true)')==1
        original=original.replace('npc_saved_identity_runtime::read_current(saved,error,true)','npc_saved_identity_runtime::read_current(saved,error)',1)
    else:
        assert original.count('PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);')==1
        original=original.replace(', bool share_metadata)',')',1).replace('PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);','PageReadScope pages;',1)
    assert hashlib.sha256(original.encode()).hexdigest()==item['hash'], 'NPC Find scope changed another owner/effect check'
action=(a.root/'src/game/npc_remove_runtime.inc').read_text(encoding='utf-8')
assert 'live_host_state::apply_timing::PageReadScope pages;' in body(action,'bool state_owners(const action::Context&')
saved=(a.root/'src/game/npc_saved_identity_runtime.cpp').read_text(encoding='utf-8')
assert 'live_host_state::apply_timing::PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);' in body(saved,'bool read_current(std::vector<Current>& out, std::string& error, bool share_metadata)')
calls=[item['new'] for item in find_scope_guards]
(a.output/'npc_find_calls.inc').write_text(
    'uint32_t action_find(Find find,uint64_t uid,uintptr_t& registered){struct {Find find;} api{find};struct {uint64_t local_uid;} op{uid};uint32_t seh=0;'+calls[0]+'return seh;}\n'+
    'uint32_t saved_find(Find find,uint64_t uid,uintptr_t& pointer){struct {Find find;} api{find};uint32_t code=0;'+calls[1]+'return code;}\n',encoding='utf-8')

# The local scope changes only metadata lifetime across the two immediate reads.
witness=(a.root/'src/game/native_open_witness.cpp').read_text(encoding='utf-8')
scope_old='    ++current.world_checks;\n    if(!world_witness(first,error))return false;\n    ++current.world_checks;\n    if(!world_witness(second,error))return false;'
scope_new='    {\n        mt2mp::game::live_host_state::apply_timing::PageReadScope pages(\n            mt2mp::game::live_host_state::apply_timing::page_read_scope==nullptr);\n        ++current.world_checks;\n        if(!world_witness(first,error))return false;\n        ++current.world_checks;\n        if(!world_witness(second,error))return false;\n    }'
actual=body(witness,'bool read_completed_world(')
assert actual.count(scope_new)==1, 'Completed-world metadata scope changed'
original=actual.replace(scope_new,scope_old,1)
assert hashlib.sha256(original.encode()).hexdigest()=='955b2a06603fb04df9c8acdd99c2a63c7900c3d78b353c568dc5aa3041b64cea', 'Completed-world reader changed outside its local metadata scope'
guards=[('bool fail(', '9b5031ed1e70c8b8bd00a2d3a94a745a573285902e33b775c3766a3110d1b526'), ('bool readable(', '54306171b323a8b0ed4b67e0d497ee58ec66fbc71a83b33e8f31be4bb494e87d'), ('DWORD copy(', '7018793e6633a9b9816aedf58895f8d1bac6ac2a8394d76e5b9e0bbc5fca8851'), ('template<class T> bool read(', '1faa597938d8e881a43d513e0ca6f3cd36fee9200d81a99e1105a461df913b16'), ('bool vt(', '495b6c9b9c75f5820292664cd1f94249973ed484c3b5c312c6a41a485575f158'), ('bool mode_singleton(', 'a0a3dac38434ae325ea430452d7d908aa05bf751c3ac763f092700024ff04a11'), ('bool world_witness(', 'aebe4a78c97de856e13d2eb2799236b4a82221e666f4bba6b35d34366db9b128')]
pieces=[]
for anchor,digest in guards:
    part=body(witness,anchor)
    baseline_part=part
    if anchor=='bool world_witness(':
        binding='#ifdef MT2MP_FRAMEWORK_PLUGIN\n    if(!framework_binding(error))return fail(error,error);\n#endif\n'
        timed_binding='#ifdef MT2MP_FRAMEWORK_PLUGIN\n    {\n        mt2mp::game::live_host_state::apply_timing::Scope timing(\n            mt2mp::game::live_host_state::apply_timing::Part::NativeOpenFrameworkBinding);\n        if(!framework_binding(error,timed_account_attributes))return fail(error,error);\n    }\n#endif\n'
        fields_timing='    mt2mp::game::live_host_state::apply_timing::Scope timing(\n        mt2mp::game::live_host_state::apply_timing::Part::NativeOpenWorldFields);\n'
        assert part.count(timed_binding)==1, 'Framework completed-world timing boundary changed'
        assert part.count(fields_timing)==1, 'Completed-world fields timing boundary changed'
        baseline_part=part.replace(timed_binding,binding,1).replace(fields_timing,'',1)
        assert baseline_part.count(binding)==1, 'Framework completed-world binding changed'
        baseline_part=baseline_part.replace(binding,'',1)
    assert hashlib.sha256(baseline_part.encode()).hexdigest()==digest, 'Completed-world dependency changed: '+anchor
    if anchor=='DWORD copy(':part=part.replace('DWORD copy(', 'DWORD native_copy(',1)
    pieces.append(part)
world_type='struct WorldWitness {void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;bool operator==(const WorldWitness&)const=default;};'
assert witness.count(world_type)==1
(a.output/'open_witness_type.inc').write_text(world_type+'\n',encoding='utf-8')
(a.output/'open_witness_bodies.inc').write_text('\n'.join(pieces)+'\n'+original.replace('bool read_completed_world(', 'bool baseline_read_completed_world(',1)+'\n'+actual+'\n',encoding='utf-8')

# Extract the complete saved reader and preserve the old independent boundary.
saved_reader=body(saved,'bool read_current(std::vector<Current>& out, std::string& error, bool share_metadata)')
baseline=prior_child_reader(saved_reader).replace(', bool share_metadata)',')',1).replace('PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);','PageReadScope pages;',1)
assert hashlib.sha256(baseline.encode()).hexdigest()=='524a94ad82498e4537635794b214019997aaec43c7d1293c2955e328f244fec5', 'Saved reader changed outside opt-in metadata lifetime'
helpers=[]
for anchor,digest in [('bool current(opened::CurrentWorld&','933d0e7e6696736310489d260080d1e68bde02df955cc33cfd45b6987dfb5a5e'),('bool same_world(','479498691688688022a6d054c8704d5cd19bd176e360a3e91a656cf6b1cb1c34'),('DWORD find_call(','3965e558d3ea5137b66ab6d4fba17b8548c340d3c993e7e8af1d8378aeac3455')]:
    selected=body(saved,anchor)
    assert hashlib.sha256(selected.encode()).hexdigest()==digest, 'Saved reader dependency drifted: '+anchor
    helpers.append(selected)
(a.output/'npc_saved_metadata_bodies.inc').write_text('\n'.join(reversed(helpers))+'\n'+baseline.replace('bool read_current(','bool baseline_read_current(',1)+'\n'+saved_reader+'\n',encoding='utf-8')
parent=body(action,'bool state_owners_impl(')
edge='if(!npc_saved_identity_runtime::read_current(saved,error,true))return false;'
assert parent.count(edge)==1
parent_baseline=parent.replace(edge,'if(!npc_saved_identity_runtime::read_current(saved,error))return false;',1)
assert hashlib.sha256(parent_baseline.encode()).hexdigest()=='df810f92fe6b7748c56f1568191c5b76886389ea4f553123c2e018ce72b420b5', 'NPC parent changed outside saved-reader opt-in'
header=(a.root/'src/game/npc_saved_identity_runtime.h').read_text(encoding='utf-8')
assert 'bool read_current(std::vector<Current>&, std::string& error, bool share_metadata=false);' in header
(a.output/'npc_saved_metadata_edge.inc').write_text('bool actual_owner_edge(std::vector<Current>& saved,std::string& error){'+edge+'return true;}\n',encoding='utf-8')

# The four diagnostic scopes leave the complete original reader unchanged.
world_prop=(a.root/'src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp').read_text(encoding='utf-8')

# Reverse only the bounded array read; all historical whole-source guards remain.
array_slot_loop = '    // Read each physical slot before growing storage; a corrupt count cannot reserve gigabytes.\n    for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!m.copy(data+static_cast<size_t>(i)*sizeof(value),&value,sizeof(value),e))return false;\n        if(!value||!unique.insert(value).second)return fail(e,"remove_reader_array_null_or_duplicate");out.push_back(value);}'
array_chunk_loop = '    // Bound temporary storage independently of the native count; every slot is still validated.\n    uintptr_t values[64]{};\n    for(size_t first=0;first<static_cast<size_t>(count);){\n        const auto slots=std::min(size_t(64),static_cast<size_t>(count)-first);\n        std::string chunk_error;\n        const bool copied=m.copy(data+first*sizeof(uintptr_t),values,slots*sizeof(uintptr_t),chunk_error);\n        for(size_t i=0;i<slots;++i){\n            // A later unreadable slot must not hide an earlier null or duplicate.\n            if(!copied&&!m.copy(data+(first+i)*sizeof(uintptr_t),&values[i],sizeof(uintptr_t),e))return false;\n            const auto value=values[i];\n            if(!value||!unique.insert(value).second)return fail(e,"remove_reader_array_null_or_duplicate");\n            out.push_back(value);\n        }\n        if(!copied){if(e.empty())e=chunk_error;return false;}\n        first+=slots;\n    }'
assert world_prop.count(array_chunk_loop)==1, 'WorldProp bounded array loop changed'
untimed=world_prop.replace(array_chunk_loop,array_slot_loop,1)
for part,indent in [('WorldPropCurrent','    '),('WorldPropRootRegion','        '),('WorldPropMainMember','    ')]:
    line=indent+'mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::'+part+');\n'
    assert untimed.count(line)==1, 'WorldProp diagnostic boundary changed: '+part
    untimed=untimed.replace(line,'',1)
typed_original='    uintptr_t found=0,casted=0;const auto seh=lookup(api_,image_,uid,0x1673900,type,0,found,casted);'
typed_timed='    uintptr_t found=0,casted=0;uint32_t seh=0;\n    {mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::WorldPropTypedLookup);\n        seh=lookup(api_,image_,uid,0x1673900,type,0,found,casted);}'
assert untimed.count(typed_timed)==1, 'WorldProp native lookup timing boundary changed'
untimed=untimed.replace(typed_timed,typed_original,1)
assert hashlib.sha256(untimed.encode()).hexdigest()=='d09b0b06cd8d4216cfad80828aa26cdcd3ddd362a72790c97fb5b0b593128fb3', 'WorldProp reader changed beyond four diagnostic scopes'
helpers=[body(world_prop,anchor)+(';' if anchor=='struct Busy ' else '') for anchor in ('bool fail(', 'struct Busy ', 'template<class T>bool at(', 'bool vt(', 'bool array(', 'bool list(', 'uint32_t lookup(')]
functions=[body(world_prop,anchor) for anchor in ('bool Reader::current(', 'bool Reader::typed(', 'bool Reader::main_member(')]
(a.output/'world_prop_attribution_bodies.inc').write_text('\n'.join(helpers+functions)+'\n',encoding='utf-8')
(a.output/'world_prop_attribution_root.inc').write_text(body(world_prop,'auto root_region=')+';\n',encoding='utf-8')

array_helpers=[body(world_prop,anchor) for anchor in ('bool fail(', 'template<class T>bool at(', 'bool array(', 'bool list(', 'uint32_t copy_native(', 'bool ProcessMemory::copy(')]
array_baseline=body(world_prop,'bool array(').replace(array_chunk_loop,array_slot_loop,1).replace('bool array(', 'bool original_array(',1)
(a.output/'world_prop_array_bodies.inc').write_text('\n'.join(array_helpers+[array_baseline])+'\n',encoding='utf-8')

# Metadata reuse ends before the Character user reader's original Find calls.
character=(a.root/'src/game/character_actor_records_runtime.inc').read_text(encoding='utf-8')
pure_call='([&]{live_host_state::apply_timing::PageReadScope definition_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return character_definition_identity::read(at,owner.image,uintptr_t(before.origin.state),manager,current,error);}())'
assert character.count(pure_call)==1
current=body(character,'bool read_current(')
assert current.index(pure_call)<current.index('character_membership_reader::users(')<current.index('!reader.current(after,true,error)')
definitions=(a.root/'src/game/character_definition_identity.h').read_bytes()
assert hashlib.sha256(definitions).hexdigest()=='6ffffcd3f95f3125115a064fb1e986748361cbf8ae85eaa473e69b2399565778', 'Pure Character reader changed; recheck callback-free extent'
(a.output/'character_definition_pure_call.inc').write_text('return '+pure_call+';\n',encoding='utf-8')

# Each extent reads live bytes. The callback-bearing users reader resets page
# metadata around its actual original Find lambda on entry, return and unwind.
subscriber=(a.root/'src/game/intervention_subscriber_live.inc').read_text(encoding='utf-8')
subscriber_call='([&]{live::apply_timing::PageReadScope subscriber_pages;return subscriber_intervention::read_all(read,copy,r.image,values,owners,error);}())'
assert subscriber.count(subscriber_call)==1
(a.output/'pure_subscriber_call.inc').write_text('return '+subscriber_call+';\n',encoding='utf-8')
release=(a.root/'src/game/release_reference_runtime.inc').read_text(encoding='utf-8')
release_baseline='    if(!release_references::base_types(read,text,r.image,uintptr_t(world.state),types,e)||!release_references::base_types(read,text,r.image,uintptr_t(world.state),checked,e)||!release_references::add_base_keys(types,checked,out,e))return false;'
release_call='    {live::apply_timing::PageReadScope base_type_pages(!live::apply_timing::PageReadScope::reader_active());\n    if(!release_references::base_types(read,text,r.image,uintptr_t(world.state),types,e)||!release_references::base_types(read,text,r.image,uintptr_t(world.state),checked,e)||!release_references::add_base_keys(types,checked,out,e))return false;\n    }'
assert release.count(release_call)==1
(a.output/'pure_release_baseline.inc').write_text(release_baseline+'\n',encoding='utf-8')
(a.output/'pure_release_call.inc').write_text(release_call+'\n',encoding='utf-8')
release_runtime=(a.root/'src/game/release_runtime.cpp').read_text(encoding='utf-8')
(a.output/'pure_release_string_bodies.inc').write_text(body(release_runtime,'template<class T>bool at(Runtime&')+'\n'+body(release_runtime,'bool string(Runtime&')+'\n',encoding='utf-8')
find_call='    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& cause){live_host_state::apply_timing::PageReadScope original_lookup(live_host_state::apply_timing::PageBoundary::OriginalCall);const auto seh=lookup(api.find,uid,pointer);\n        return !seh||fail(cause,"Character complete census current FindUID failed SEH="+std::to_string(seh));};'
users_call='([&]{live_host_state::apply_timing::PageReadScope user_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return character_membership_reader::users(at,find,owner.image,current,current_users,error);}())'
assert character.count(find_call)==1 and character.count(users_call)==1
assert 'PageBoundary::OriginalCall);const auto seh=lookup(api.find,uid,pointer);' in find_call
assert current.index(find_call)<current.index(users_call)<current.index('!reader.current(after,true,error)')
(a.output/'pure_users_find.inc').write_text(find_call+'\n',encoding='utf-8')
(a.output/'pure_users_call.inc').write_text('return '+users_call+';\n',encoding='utf-8')
for relative,digest in [
    ('src/game/character_membership_reader.h','762605d894cf728dbe2f869bc3f0128d66e05be53ef137a63ff9e75e43643cac'),
    ('src/game/subscriber_intervention.h','c5f4dc49e734e3bb589b93f6a88ab7f63e8665d4e96551659ac2619929fc2cc7'),
    ('src/game/release_reference_reader.h','07d5be40cd7f81fe0133674bbebb3e5b89471ad39f4b46c63470abd78980f80a')]:
    assert hashlib.sha256((a.root/relative).read_bytes()).hexdigest()==digest, 'Reader callbacks changed; recheck bounded metadata extent: '+relative

# Extract each complete production loop, including both full validations.
for relative,output in [
    ('src/game/boss_current.cpp','boss_region_index.inc'),
    ('src/game/release_reference_runtime.inc','release_region_index.inc'),
]:
    source=(a.root/relative).read_text(encoding='utf-8')
    start=source.index('    region_saved_identity::Index region_index;')
    end=source.index('return false;',source.index('    if(!region_saved_identity::verify(',start))+len('return false;')
    (a.output/output).write_text(source[start:end]+'\n',encoding='utf-8')

# Full Dungeon roster traversal shares page metadata only between native callbacks.
dungeon=(a.root/'src/game/dungeon_scene_identity_runtime.cpp').read_text(encoding='utf-8')
assert dungeon.count('([&]{live_host_state::apply_timing::PageReadScope roster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return dungeon_scene_roster::read(at,typed,find,identify,image,uintptr_t(world.map),root,scenes,error);}())')==1
assert dungeon.count('([&]{live_host_state::apply_timing::PageReadScope roster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return dungeon_scene_roster::read(at,typed,find,identify,image,uintptr_t(world.map),root,after,error);}())')==1
assert dungeon.count('([&]{live_host_state::apply_timing::PageReadScope roster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return dungeon_scene_roster::read(at,typed,find,identify,image,map,map+0x1c0,ready,error);}())')==1
assert hashlib.sha256((a.root/'src/game/dungeon_scene_roster.h').read_bytes()).hexdigest()=='de5cc6413dc56dd13069aa7f591aa01fbf3022b27fdd588df6ece9f3cdfab85d'
assert hashlib.sha256((a.root/'src/game/dungeon_scene_key.h').read_bytes()).hexdigest()=='74927c702581393555223b03acf9587748dcec2a33ef48303f051ab2b1fbb940'
exterior_loop='    {live_host_state::apply_timing::PageReadScope exterior_pages(!live_host_state::apply_timing::PageReadScope::reader_active());\n    dungeon_scene_roster::Array regions;const auto map=uintptr_t(world.map);\n    if(!dungeon_scene_roster::array(at,map+0x230,regions,error))return false;\n    for(const auto region:regions.members){uintptr_t manager=0;if(!at(region,0x950,manager,error))return false;\n        dungeon_scene_roster::Array buildings;if(!dungeon_scene_roster::array(at,manager+0x408,buildings,error))return false;\n        for(const auto building:buildings.members){uintptr_t dungeon=0;uint64_t uid=0;if(!at(building,0xa38,dungeon,error))return false;\n            if(!dungeon)continue;Exterior row;if(!at(building,0x18,uid,error)||!([&]{live_host_state::apply_timing::PageReadScope original_key(live_host_state::apply_timing::PageBoundary::OriginalCall);return remove_diagnostics::resolve_current_building_key(snapshot,building,uid,row.record,error);}()))return false;\n            row.uid=uid;exteriors.push_back(std::move(row));}\n        if(!dungeon_scene_roster::unchanged(at,manager+0x408,buildings,error))return false;\n    }\n    if(!dungeon_scene_roster::unchanged(at,map+0x230,regions,error))return false;\n    }\n'
assert dungeon.count(exterior_loop)==1
(a.output/'dungeon_exterior_loop.inc').write_text(exterior_loop,encoding='utf-8')
(a.output/'dungeon_exterior_baseline.inc').write_text('    dungeon_scene_roster::Array regions;const auto map=uintptr_t(world.map);\n    if(!dungeon_scene_roster::array(at,map+0x230,regions,error))return false;\n    for(const auto region:regions.members){uintptr_t manager=0;if(!at(region,0x950,manager,error))return false;\n        dungeon_scene_roster::Array buildings;if(!dungeon_scene_roster::array(at,manager+0x408,buildings,error))return false;\n        for(const auto building:buildings.members){uintptr_t dungeon=0;uint64_t uid=0;if(!at(building,0xa38,dungeon,error))return false;\n            if(!dungeon)continue;Exterior row;if(!at(building,0x18,uid,error)||!remove_diagnostics::resolve_current_building_key(snapshot,building,uid,row.record,error))return false;\n            row.uid=uid;exteriors.push_back(std::move(row));}\n        if(!dungeon_scene_roster::unchanged(at,manager+0x408,buildings,error))return false;\n    }\n    if(!dungeon_scene_roster::unchanged(at,map+0x230,regions,error))return false;\n',encoding='utf-8')
(a.output/'dungeon_roster_scoped_call.inc').write_text('return '+'([&]{live_host_state::apply_timing::PageReadScope roster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return dungeon_scene_roster::read(at,typed,find,identify,image,map,map+0x1c0,ready,error);}())'+';\n',encoding='utf-8')
dungeon_callback=body(body(dungeon,'static bool read_census('),'auto find=[&](')+';'
assert dungeon_callback=='auto find=[&](uint64_t uid,uintptr_t& found,std::string& e){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);\n        if(!uid)return fail(e,"Dungeon scene identity requires full native UID");const auto exception=original_find(api.find,uid,found);\n        return !exception||fail(e,"Dungeon scene original full UID lookup SEH="+std::to_string(exception));\n    };'
(a.output/'dungeon_census_find.inc').write_text(dungeon_callback+'\n',encoding='utf-8')
dungeon_callback=body(body(dungeon,'static bool read_census('),'auto typed=[&](')+';'
assert dungeon_callback=='auto typed=[&](uintptr_t object,uintptr_t source,uintptr_t target,std::string& e){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);uintptr_t found=0;\n        const auto exception=original_cast(api.cast,object,image+source,image+target,found);\n        return (!exception&&found==object)||fail(e,"Dungeon scene original RTTI receiver differs SEH="+std::to_string(exception));\n    };'
(a.output/'dungeon_census_typed.inc').write_text(dungeon_callback+'\n',encoding='utf-8')
dungeon_callback=body(body(dungeon,'bool read_scene_roster('),'auto find=[&](')+';'
assert dungeon_callback=='auto find=[&](uint64_t uid,uintptr_t& found,std::string& e){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);\n        if(!uid)return fail(e,"Dungeon current scene requires full native UID");\n        const auto exception=original_find(api.find,uid,found);\n        return !exception||fail(e,"Dungeon current scene full UID lookup SEH="+std::to_string(exception));\n    };'
(a.output/'dungeon_current_find.inc').write_text(dungeon_callback+'\n',encoding='utf-8')
dungeon_callback=body(body(dungeon,'bool read_scene_roster('),'auto typed=[&](')+';'
assert dungeon_callback=='auto typed=[&](uintptr_t object,uintptr_t from,uintptr_t to,std::string& e){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);uintptr_t found=0;\n        const auto exception=original_cast(api.cast,object,image+from,image+to,found);\n        return (!exception&&found==object)||fail(e,"Dungeon current scene original RTTI differs SEH="+std::to_string(exception));\n    };'
(a.output/'dungeon_current_typed.inc').write_text(dungeon_callback+'\n',encoding='utf-8')

# Party manager sampling owns one bounded metadata extent; Find invalidates it.
party=body((a.root/'src/game/party_saved_identity_runtime.cpp').read_text(encoding='utf-8'),'bool collect_current_raw(')
party_loop='{live_host_state::apply_timing::PageReadScope pool_pages;\n    for(const auto& [shard,region]:owners){\n        dungeon_party_census::Pool pool;\n        if(!dungeon_party_census::read(memory,find,image,shard,region,pool,error))return false;\n        if(!managers.insert(pool.manager).second)return q::fail(error,"Party complete current owner census repeats a manager");\n        ready.pools.push_back(pool);\n        for(const auto& row:pool.members){\n            const auto [where,inserted]=members.emplace(row.uid,row);\n            if(!inserted&&where->second!=row)return q::fail(error,"Party transfer has inconsistent actual full-UID owner fields");\n        }\n    }\n    }'
assert party.count(party_loop)==1
party_find=body(party,'auto find=[&](uint64_t uid,uintptr_t& found,std::string& cause){')+';'
assert party_find=='auto find=[&](uint64_t uid,uintptr_t& found,std::string& cause){live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);\n        const auto code=exact_uid(uid,found);return !code||q::fail(cause,"Party original full UID lookup SEH="+std::to_string(code));\n    };'
(a.output/'party_pool_loop.inc').write_text(party_loop+'\n',encoding='utf-8')
(a.output/'party_pool_baseline.inc').write_text('for(const auto& [shard,region]:owners){\n        dungeon_party_census::Pool pool;\n        if(!dungeon_party_census::read(memory,find,image,shard,region,pool,error))return false;\n        if(!managers.insert(pool.manager).second)return q::fail(error,"Party complete current owner census repeats a manager");\n        ready.pools.push_back(pool);\n        for(const auto& row:pool.members){\n            const auto [where,inserted]=members.emplace(row.uid,row);\n            if(!inserted&&where->second!=row)return q::fail(error,"Party transfer has inconsistent actual full-UID owner fields");\n        }\n    }'+'\n',encoding='utf-8')
(a.output/'party_pool_find.inc').write_text(party_find+'\n',encoding='utf-8')
assert hashlib.sha256((a.root/'src/game/dungeon_party_census.h').read_bytes()).hexdigest()=='3681665f2c03cdf70bc8602db813844b073ed8e1641dada352cd64744bf6241e', 'Party pool reader changed; recheck original callback extent'
monster=(a.root/'src/game/intervention_census.cpp').read_text(encoding='utf-8')
assert monster.count('([&]{live_host_state::apply_timing::PageReadScope monster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return monster_roster(memory,image,map,scope,monsters,error);}())')==1
(a.output/'shared_monster_call.inc').write_text('return '+'([&]{live_host_state::apply_timing::PageReadScope monster_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return monster_roster(memory,image,map,scope,monsters,error);}())'+';\n',encoding='utf-8')
boss=(a.root/'src/game/boss_current.cpp').read_text(encoding='utf-8')
assert boss.count('([&]{live_host_state::apply_timing::PageReadScope boss_pool_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return character_boss_retirement_read::census(image,shard,region,at,find,pool.native,error);}())')==1
assert boss.count('bool operator()(uint64_t uid,uintptr_t& found,std::string& error)const{live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);const auto exception=lookup(image,uid,found);return !exception||fail(error,"Boss current full UID lookup SEH="+std::to_string(exception));}')==1
(a.output/'shared_boss_call.inc').write_text('return '+'([&]{live_host_state::apply_timing::PageReadScope boss_pool_pages(!live_host_state::apply_timing::PageReadScope::reader_active());return character_boss_retirement_read::census(image,shard,region,at,find,pool.native,error);}())'+';\n',encoding='utf-8')
(a.output/'shared_boss_find.inc').write_text('bool operator()(uint64_t uid,uintptr_t& found,std::string& error)const{live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);const auto exception=lookup(image,uid,found);return !exception||fail(error,"Boss current full UID lookup SEH="+std::to_string(exception));}'+'\n',encoding='utf-8')
building=(a.root/'src/game/root_saved_building.h').read_text(encoding='utf-8')
assert building.count('inline bool region_id(uintptr_t image, void* map, void* region, void* manager, void* shard, int32_t& id, std::string& error) {\n    mt2mp::game::live_host_state::apply_timing::PageReadScope region_pages;\n    if (!type(image, map, 0x1742860, error)) return false;\n    auto* root = static_cast<char*>(map) + 0x1c0;\n    int32_t shard_type = -1;\n    if (shard != root || !type(image, shard, 0x174ad20, error) || !read(shard, 0x48, shard_type, error) || shard_type != 0) {\n        error = "inline building owner is not supported saved root shard"; return false;\n    }\n    void* region_shard = nullptr; void* owner = nullptr; void* reverse_region = nullptr; void* reverse_shard = nullptr;\n    if (!type(image, region, 0x1787930, error) || !type(image, manager, 0x17183c0, error) ||\n        !read(region, 0x70, region_shard, error) || !read(region, 0x950, owner, error) ||\n        !read(manager, 0x450, reverse_shard, error) || !read(manager, 0x458, reverse_region, error)) return false;\n    if (region_shard != shard || owner != manager || reverse_shard != shard || reverse_region != region) {\n        error = "inline root region/manager/shard ownership differs"; return false;\n    }\n    auto* saved = static_cast<char*>(map) + 0x230;\n    void* data = nullptr; void* owned = nullptr; int32_t count = 0, capacity = 0, owned_count = 0, owned_capacity = 0;\n    if (!type(image, saved, 0x16f04e0, error) || !read(saved, 8, data, error) || !read(saved, 0x10, count, error) ||\n        !read(saved, 0x14, capacity, error) || !read(map, 0x358, owned, error) || !read(map, 0x360, owned_count, error) ||\n        !read(map, 0x364, owned_capacity, error)) return false;\n    if (count < 0 || count > 4096 || capacity < count || owned_count < 0 || owned_count > 4096 || owned_capacity < owned_count) {\n        error = "inline root region array bounds invalid required_max=4096"; return false;\n    }\n    std::set<void*> owned_regions;\n    for (int32_t index = 0; index < owned_count; ++index) {\n        void* candidate = nullptr; void* candidate_shard = nullptr;\n        if (!read(owned, size_t(index) * 8, candidate, error) || !type(image, candidate, 0x1787930, error) ||\n            !read(candidate, 0x70, candidate_shard, error)) return false;\n        if (candidate_shard != shard || !owned_regions.insert(candidate).second) { error = "inline root owned region shard or uniqueness differs"; return false; }\n    }\n    std::set<int32_t> ids; std::set<void*> saved_regions; bool found = false;\n    for (int32_t index = 0; index < count; ++index) {\n        void* candidate = nullptr; void* candidate_shard = nullptr; int32_t candidate_id = -1;\n        if (!read(data, size_t(index) * 8, candidate, error) || !type(image, candidate, 0x1787930, error) ||\n            !read(candidate, 0x70, candidate_shard, error) || !read(candidate, 0x834, candidate_id, error)) return false;\n        const auto context = " saved_index=" + std::to_string(index) + " region_id=" + std::to_string(candidate_id) +\n            " saved_count=" + std::to_string(count) + " owned_count=" + std::to_string(owned_count);\n        if (candidate_shard != shard) { error = "inline saved root region shard differs required=root" + context; return false; }\n        if (candidate_id < 0) { error = "inline saved root region ID invalid required_min=0" + context; return false; }\n        if (!saved_regions.insert(candidate).second) { error = "inline saved root region pointer repeated required_unique=1" + context; return false; }\n        if (!ids.insert(candidate_id).second) { error = "inline saved root region ID repeated required_unique=1" + context; return false; }\n        if (candidate == region) { found = true; id = candidate_id; }\n    }\n    for (int32_t index = 0; index < owned_count; ++index) {\n        void* candidate = nullptr;\n        if (!read(owned, size_t(index) * 8, candidate, error)) return false;\n        if (!saved_regions.contains(candidate)) {\n            error = "inline root owned region absent from saved Map.region required_membership=1 owned_index=" + std::to_string(index) +\n                " saved_count=" + std::to_string(count) + " owned_count=" + std::to_string(owned_count); return false;\n        }\n    }\n    if (!found) { error = "inline root region absent from saved Map.region"; return false; }\n    return true;\n}')==1
(a.output/'shared_building_baseline.inc').write_text('inline bool unscoped_region_id(uintptr_t image, void* map, void* region, void* manager, void* shard, int32_t& id, std::string& error) {\n    if (!type(image, map, 0x1742860, error)) return false;\n    auto* root = static_cast<char*>(map) + 0x1c0;\n    int32_t shard_type = -1;\n    if (shard != root || !type(image, shard, 0x174ad20, error) || !read(shard, 0x48, shard_type, error) || shard_type != 0) {\n        error = "inline building owner is not supported saved root shard"; return false;\n    }\n    void* region_shard = nullptr; void* owner = nullptr; void* reverse_region = nullptr; void* reverse_shard = nullptr;\n    if (!type(image, region, 0x1787930, error) || !type(image, manager, 0x17183c0, error) ||\n        !read(region, 0x70, region_shard, error) || !read(region, 0x950, owner, error) ||\n        !read(manager, 0x450, reverse_shard, error) || !read(manager, 0x458, reverse_region, error)) return false;\n    if (region_shard != shard || owner != manager || reverse_shard != shard || reverse_region != region) {\n        error = "inline root region/manager/shard ownership differs"; return false;\n    }\n    auto* saved = static_cast<char*>(map) + 0x230;\n    void* data = nullptr; void* owned = nullptr; int32_t count = 0, capacity = 0, owned_count = 0, owned_capacity = 0;\n    if (!type(image, saved, 0x16f04e0, error) || !read(saved, 8, data, error) || !read(saved, 0x10, count, error) ||\n        !read(saved, 0x14, capacity, error) || !read(map, 0x358, owned, error) || !read(map, 0x360, owned_count, error) ||\n        !read(map, 0x364, owned_capacity, error)) return false;\n    if (count < 0 || count > 4096 || capacity < count || owned_count < 0 || owned_count > 4096 || owned_capacity < owned_count) {\n        error = "inline root region array bounds invalid required_max=4096"; return false;\n    }\n    std::set<void*> owned_regions;\n    for (int32_t index = 0; index < owned_count; ++index) {\n        void* candidate = nullptr; void* candidate_shard = nullptr;\n        if (!read(owned, size_t(index) * 8, candidate, error) || !type(image, candidate, 0x1787930, error) ||\n            !read(candidate, 0x70, candidate_shard, error)) return false;\n        if (candidate_shard != shard || !owned_regions.insert(candidate).second) { error = "inline root owned region shard or uniqueness differs"; return false; }\n    }\n    std::set<int32_t> ids; std::set<void*> saved_regions; bool found = false;\n    for (int32_t index = 0; index < count; ++index) {\n        void* candidate = nullptr; void* candidate_shard = nullptr; int32_t candidate_id = -1;\n        if (!read(data, size_t(index) * 8, candidate, error) || !type(image, candidate, 0x1787930, error) ||\n            !read(candidate, 0x70, candidate_shard, error) || !read(candidate, 0x834, candidate_id, error)) return false;\n        const auto context = " saved_index=" + std::to_string(index) + " region_id=" + std::to_string(candidate_id) +\n            " saved_count=" + std::to_string(count) + " owned_count=" + std::to_string(owned_count);\n        if (candidate_shard != shard) { error = "inline saved root region shard differs required=root" + context; return false; }\n        if (candidate_id < 0) { error = "inline saved root region ID invalid required_min=0" + context; return false; }\n        if (!saved_regions.insert(candidate).second) { error = "inline saved root region pointer repeated required_unique=1" + context; return false; }\n        if (!ids.insert(candidate_id).second) { error = "inline saved root region ID repeated required_unique=1" + context; return false; }\n        if (candidate == region) { found = true; id = candidate_id; }\n    }\n    for (int32_t index = 0; index < owned_count; ++index) {\n        void* candidate = nullptr;\n        if (!read(owned, size_t(index) * 8, candidate, error)) return false;\n        if (!saved_regions.contains(candidate)) {\n            error = "inline root owned region absent from saved Map.region required_membership=1 owned_index=" + std::to_string(index) +\n                " saved_count=" + std::to_string(count) + " owned_count=" + std::to_string(owned_count); return false;\n        }\n    }\n    if (!found) { error = "inline root region absent from saved Map.region"; return false; }\n    return true;\n}'+'\n',encoding='utf-8')
assert body(building,'inline bool type(')=='inline bool type(uintptr_t image, const void* object, uintptr_t expected, std::string& error) {\n    uintptr_t actual = 0;\n    if (!read(object, 0, actual, error)) return false;\n    if (actual != image + expected) { error = "root saved vtable differs expected_rva=" + std::to_string(expected); return false; }\n    return true;\n}'
(a.output/'shared_building_type.inc').write_text('inline bool type(uintptr_t image, const void* object, uintptr_t expected, std::string& error) {\n    uintptr_t actual = 0;\n    if (!read(object, 0, actual, error)) return false;\n    if (actual != image + expected) { error = "root saved vtable differs expected_rva=" + std::to_string(expected); return false; }\n    return true;\n}'+'\n',encoding='utf-8')
(a.output/'shared_building_candidate.inc').write_text('inline bool instrumented_region_id(uintptr_t image, void* map, void* region, void* manager, void* shard, int32_t& id, std::string& error) {\n    mt2mp::game::live_host_state::apply_timing::PageReadScope region_pages;\n    if (!type(image, map, 0x1742860, error)) return false;\n    auto* root = static_cast<char*>(map) + 0x1c0;\n    int32_t shard_type = -1;\n    if (shard != root || !type(image, shard, 0x174ad20, error) || !read(shard, 0x48, shard_type, error) || shard_type != 0) {\n        error = "inline building owner is not supported saved root shard"; return false;\n    }\n    void* region_shard = nullptr; void* owner = nullptr; void* reverse_region = nullptr; void* reverse_shard = nullptr;\n    if (!type(image, region, 0x1787930, error) || !type(image, manager, 0x17183c0, error) ||\n        !read(region, 0x70, region_shard, error) || !read(region, 0x950, owner, error) ||\n        !read(manager, 0x450, reverse_shard, error) || !read(manager, 0x458, reverse_region, error)) return false;\n    if (region_shard != shard || owner != manager || reverse_shard != shard || reverse_region != region) {\n        error = "inline root region/manager/shard ownership differs"; return false;\n    }\n    auto* saved = static_cast<char*>(map) + 0x230;\n    void* data = nullptr; void* owned = nullptr; int32_t count = 0, capacity = 0, owned_count = 0, owned_capacity = 0;\n    if (!type(image, saved, 0x16f04e0, error) || !read(saved, 8, data, error) || !read(saved, 0x10, count, error) ||\n        !read(saved, 0x14, capacity, error) || !read(map, 0x358, owned, error) || !read(map, 0x360, owned_count, error) ||\n        !read(map, 0x364, owned_capacity, error)) return false;\n    if (count < 0 || count > 4096 || capacity < count || owned_count < 0 || owned_count > 4096 || owned_capacity < owned_count) {\n        error = "inline root region array bounds invalid required_max=4096"; return false;\n    }\n    std::set<void*> owned_regions;\n    for (int32_t index = 0; index < owned_count; ++index) {\n        void* candidate = nullptr; void* candidate_shard = nullptr;\n        if (!read(owned, size_t(index) * 8, candidate, error) || !type(image, candidate, 0x1787930, error) ||\n            !read(candidate, 0x70, candidate_shard, error)) return false;\n        if (candidate_shard != shard || !owned_regions.insert(candidate).second) { error = "inline root owned region shard or uniqueness differs"; return false; }\n    }\n    std::set<int32_t> ids; std::set<void*> saved_regions; bool found = false;\n    for (int32_t index = 0; index < count; ++index) {\n        void* candidate = nullptr; void* candidate_shard = nullptr; int32_t candidate_id = -1;\n        if (!read(data, size_t(index) * 8, candidate, error) || !type(image, candidate, 0x1787930, error) ||\n            !read(candidate, 0x70, candidate_shard, error) || !read(candidate, 0x834, candidate_id, error)) return false;\n        const auto context = " saved_index=" + std::to_string(index) + " region_id=" + std::to_string(candidate_id) +\n            " saved_count=" + std::to_string(count) + " owned_count=" + std::to_string(owned_count);\n        if (candidate_shard != shard) { error = "inline saved root region shard differs required=root" + context; return false; }\n        if (candidate_id < 0) { error = "inline saved root region ID invalid required_min=0" + context; return false; }\n        if (!saved_regions.insert(candidate).second) { error = "inline saved root region pointer repeated required_unique=1" + context; return false; }\n        if (!ids.insert(candidate_id).second) { error = "inline saved root region ID repeated required_unique=1" + context; return false; }\n        if (candidate == region) { found = true; id = candidate_id; }\n    }\n    for (int32_t index = 0; index < owned_count; ++index) {\n        void* candidate = nullptr;\n        if (!read(owned, size_t(index) * 8, candidate, error)) return false;\n        if (!saved_regions.contains(candidate)) {\n            error = "inline root owned region absent from saved Map.region required_membership=1 owned_index=" + std::to_string(index) +\n                " saved_count=" + std::to_string(count) + " owned_count=" + std::to_string(owned_count); return false;\n        }\n    }\n    if (!found) { error = "inline root region absent from saved Map.region"; return false; }\n    return true;\n}'+'\n',encoding='utf-8')
assert hashlib.sha256((a.root/'src/game/intervention_monster_roster.h').read_bytes()).hexdigest()=='4e5136069863a456cdbe2f0b28491fbc3c1026c893c5614d33392658a24fc587', 'Shared census pure reader changed'
assert hashlib.sha256((a.root/'src/game/monster_zone_read.h').read_bytes()).hexdigest()=='3c52a79c7025ad264eeb1aa08e563ac0b08a4a17af2b023ebdfd07a90bf03ce9', 'Shared census pure reader changed'
assert hashlib.sha256((a.root/'src/game/character_boss_retirement_read.h').read_bytes()).hexdigest()=='40399c4b9300c15a82cb9fb5e25500e71d836e4642548606be35272152e0bdcb', 'Shared census pure reader changed'

# Compile the actual canonical-reference double read and composed Boss admissions.
project=(a.root/'src/game/general_project_runtime.cpp').read_text(encoding='utf-8')
(a.output/'project_reference_snapshot.inc').write_text(body(project,'bool reference_snapshot(')+'\n',encoding='utf-8')
boss=(a.root/'src/game/boss_current.cpp').read_text(encoding='utf-8')
pieces=[body(boss,anchor) for anchor in ('bool same(', 'bool admitted_characters(', 'bool admitted(', 'static bool read_impl(', 'bool read(const action_session::Context&', 'bool read_with_characters(')]
(a.output/'composed_character_bodies.inc').write_text('\n'.join(pieces)+'\n',encoding='utf-8')
intervention=(a.root/'src/game/intervention_census.cpp').read_text(encoding='utf-8')
edge='out={};Census next;if(!boss_current::read_with_characters(context,binding,next.bosses,next.definitions,next.users,error))return false;'
assert intervention.count(edge)==1, 'Intervention must consume the actual first Boss Character census'
(a.output/'composed_character_edge.inc').write_text('bool actual_intervention_edge(const action_session::Context& context,const action_session::Binding& binding,Census& out,std::string& error){'+edge+'out=std::move(next);return true;}\n',encoding='utf-8')
