from pathlib import Path
import argparse,hashlib

p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
text=(a.root/'src/game/live_host_state/day_rollover/economy/monetary.cpp').read_text()
def body(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]

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

# Only the two hash-bound, read-only NPC FindUID calls omit metadata invalidation.
find_scope_guards=[{'path': 'src/game/npc_remove_runtime.inc', 'anchor': 'bool state_owners_impl(', 'old': '{live_host_state::apply_timing::PageReadScope original_lookup(live_host_state::apply_timing::PageBoundary::OriginalCall);seh=lookup(api.find,op.local_uid,registered);}', 'new': 'seh=lookup(api.find,op.local_uid,registered);', 'hash': '2eac164a369ef3932a373eee7457275b82c24716f65252fe49cbdfefb5352421'}, {'path': 'src/game/npc_saved_identity_runtime.cpp', 'anchor': 'bool read_current(std::vector<Current>& out, std::string& error, bool share_metadata)', 'old': '{live_host_state::apply_timing::PageReadScope original_lookup(live_host_state::apply_timing::PageBoundary::OriginalCall);code = find_call(api.find, uid, pointer);}', 'new': 'code = find_call(api.find, uid, pointer);', 'hash': 'e814327eb7f53bd89bc42e3c84929937a971e0f6600097610a7c96ea3bd55d3d'}]
for item in find_scope_guards:
    source=(a.root/item['path']).read_text(encoding='utf-8')
    actual=body(source,item['anchor'])
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
baseline=saved_reader.replace(', bool share_metadata)',')',1).replace('PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);','PageReadScope pages;',1)
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
