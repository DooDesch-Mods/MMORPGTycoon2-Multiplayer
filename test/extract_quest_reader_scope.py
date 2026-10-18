from pathlib import Path
import argparse,hashlib

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, required=True)
p.add_argument('--dependencies', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--root-region-source',type=Path)
p.add_argument('--root-region-baseline',type=Path)
p.add_argument('--root-region-only',action='store_true')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

def body(path, anchor):
    text = path.read_text(encoding='utf-8')
    start = text.index(anchor)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

selected=a.root_region_source or a.root/'src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp'
candidate_lambda=body(selected,'auto root_region=')
scope_line='        mt2mp::game::live_host_state::apply_timing::PageReadScope pages;\n'
root_timing_line='        mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::WorldPropRootRegion);\n'
assert candidate_lambda.count(root_timing_line)==1, 'Expected exactly the root-region diagnostic scope'
pure_lambda=candidate_lambda.replace(root_timing_line,'',1)
assert candidate_lambda.count(scope_line)==1, 'Expected the exact scoped root-region lambda'
assert hashlib.sha256(pure_lambda.encode()).hexdigest()=='4425dd3469cf53498d2595c3588a38dfa6b8fea780d53b7da0a3c1e58ccb3c8f', 'Scoped root-region reader changed beyond its diagnostic scope'
baseline_lambda=candidate_lambda.replace(scope_line,'',1)
assert hashlib.sha256(baseline_lambda.replace(root_timing_line,'',1).encode()).hexdigest()=='8b2fb74424b372da0c4be167cf35bd87f88f8b2fe83ca6e97a58135b0ba61b24', 'Unscoped reference must differ only by its metadata and diagnostic scope lines'
assert all(word not in candidate_lambda for word in ('lookup(', 'typed(', 'compare_fixed', 'current(', 'VirtualProtect', 'store(')), 'Pure root lambda boundary changed'
anchors=('bool fail(', 'template<class T>bool at(', 'bool vt(', 'bool array(', 'bool list(')
copy_anchors=('uint32_t copy_native(', 'bool ProcessMemory::copy(')
helpers='\n'.join(body(selected,anchor) for anchor in anchors)
copies='\n'.join(body(selected,anchor) for anchor in copy_anchors)

# Reverse only the bounded array read; all historical whole-source guards remain.
array_slot_loop = '    // Read each physical slot before growing storage; a corrupt count cannot reserve gigabytes.\n    for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!m.copy(data+static_cast<size_t>(i)*sizeof(value),&value,sizeof(value),e))return false;\n        if(!value||!unique.insert(value).second)return fail(e,"remove_reader_array_null_or_duplicate");out.push_back(value);}'
array_chunk_loop = '    // Bound temporary storage independently of the native count; every slot is still validated.\n    uintptr_t values[64]{};\n    for(size_t first=0;first<static_cast<size_t>(count);){\n        const auto slots=std::min(size_t(64),static_cast<size_t>(count)-first);\n        std::string chunk_error;\n        const bool copied=m.copy(data+first*sizeof(uintptr_t),values,slots*sizeof(uintptr_t),chunk_error);\n        for(size_t i=0;i<slots;++i){\n            // A later unreadable slot must not hide an earlier null or duplicate.\n            if(!copied&&!m.copy(data+(first+i)*sizeof(uintptr_t),&values[i],sizeof(uintptr_t),e))return false;\n            const auto value=values[i];\n            if(!value||!unique.insert(value).second)return fail(e,"remove_reader_array_null_or_duplicate");\n            out.push_back(value);\n        }\n        if(!copied){if(e.empty())e=chunk_error;return false;}\n        first+=slots;\n    }'
if a.root_region_baseline:
    original=a.root_region_baseline
    candidate_text=selected.read_text(encoding='utf-8')
    assert candidate_text.count(array_chunk_loop)==1, 'WorldProp bounded array loop changed'
    candidate_text=candidate_text.replace(array_chunk_loop,array_slot_loop,1)
    candidate_text=candidate_text.replace(root_timing_line,'',1)
    for part in ('WorldPropCurrent','WorldPropMainMember'):
        candidate_text=candidate_text.replace('    mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::'+part+');\n','',1)
    candidate_text=candidate_text.replace('    uintptr_t found=0,casted=0;uint32_t seh=0;\n    {mt2mp::game::live_host_state::apply_timing::Scope measured(mt2mp::game::live_host_state::apply_timing::Part::WorldPropTypedLookup);\n        seh=lookup(api_,image_,uid,0x1673900,type,0,found,casted);}', '    uintptr_t found=0,casted=0;const auto seh=lookup(api_,image_,uid,0x1673900,type,0,found,casted);',1)
    original_text=original.read_text(encoding='utf-8')
    old='auto root_region=[&](uintptr_t r){std::vector<uintptr_t> a,b;'
    new='auto root_region=[&](uintptr_t r){\n        mt2mp::game::live_host_state::apply_timing::PageReadScope pages;\n        std::vector<uintptr_t> a,b;'
    assert original_text.count(old)==1 and candidate_text==original_text.replace(old,new,1), 'Only the exact pure root lambda gains a scope'
    old_helpers='\n'.join(body(original,anchor) for anchor in anchors)
    assert helpers.replace(array_chunk_loop,array_slot_loop,1)==old_helpers, 'Actual readers changed beyond bounded array copies'
    assert copies=='\n'.join(body(original,anchor) for anchor in copy_anchors)
for name,value in {'host_copy.inc':copies,'root_region_helpers.inc':helpers,
                   'root_region_original.inc':baseline_lambda+';', 'root_region_scoped.inc':candidate_lambda+';'}.items():
    (a.output/name).write_text(value+'\n',encoding='utf-8')
if a.root_region_only:
    print('Actual root lambda/list/vt/at/array/copy extraction passed; no C++ executed')
    raise SystemExit(0)

host = a.root / 'src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp'
root = a.root / 'src/game/root_saved_building.h'
placement = a.dependencies / 'src/game/placement_ui_probe/ui_probe.cpp'
owners = a.root / 'src/game/npc_remove_runtime.inc'
saved = a.root / 'src/game/npc_saved_identity_runtime.cpp'
quest = a.root / 'src/game/live_host_state/day_rollover/economy/native_quest_state.inc'
(a.output / 'host_copy.inc').write_text(body(host, 'uint32_t copy_native(') + '\n' + body(host, 'bool ProcessMemory::copy('))
(a.output / 'root_copy.inc').write_text(body(root, 'inline DWORD copy(') + '\n' + body(root, 'inline bool bytes('))
(a.output / 'placement_copy.inc').write_text(body(placement, 'std::uint32_t copy_seh(') + '\n' + body(placement, 'bool ProcessMemory::copy('))
(a.output / 'state_owners.inc').write_text(body(owners, 'bool state_owners('))
(a.output / 'quest_npcs.inc').write_text(body(quest, 'bool quest_npc(')+'\n'+body(quest, 'bool quest_npcs('))
motion=a.root/'src/game/live_host_state/day_rollover/economy/native_monetary.cpp'
motion_once=body(motion,'bool NativePort::motion_once(')
motion_read=body(motion,'bool NativePort::read_motion(')
assert motion_once.count('apply_timing::PageReadScope pages(!apply_timing::page_read_scope);')==1
assert motion_read.count('apply_timing::PageReadScope pages;')==1
old_once=motion_once.replace('apply_timing::PageReadScope pages(!apply_timing::page_read_scope);','apply_timing::PageReadScope pages;',1)
assert hashlib.sha256(old_once.encode()).hexdigest()=='1b43f613921d2afb7b2203f96032eddc8b5021baf96145633a45b186900bc2e1'
old_read=motion_read.replace('    apply_timing::PageReadScope pages;\n','',1)
(a.output/'motion_double_scoped.inc').write_text(motion_once+'\n'+motion_read+'\n',encoding='utf-8')
(a.output/'motion_double_original.inc').write_text((old_once+'\n'+old_read).replace('motion_once(', 'motion_once_original(').replace('read_motion(', 'read_motion_original(')+'\n',encoding='utf-8')

# Mutation bodies retain their existing boundary. No Apply-wide cache is allowed.
quest_store=body(quest, 'bool NativePort::store_quest_state(')
census=body(quest, '{apply_timing::PageReadScope admission_pages;')
prefix='{apply_timing::PageReadScope admission_pages;\n'
assert census.startswith(prefix) and census.endswith('            }')
# Only the original read-only census can share page metadata. Its scope ends
# before identity publication and before any native callback or scalar CAS.
assert hashlib.sha256(census[len(prefix):-13].encode()).hexdigest()=="b3b87b756a4c8e92fb99339bb6d146474ad3a934d37a6003aa0e54bf65cce711", 'Quest admission reader body changed'
pointers=body(quest, 'const auto pointers=')
scope_line='\n        apply_timing::PageReadScope pointer_pages(!scalar_only||!apply_timing::page_read_scope);'
assert pointers.count(scope_line)==1
unscoped=pointers.replace(scope_line,'',1).replace(',npc_quest_visitor_reader::ArrayHeader* captured=nullptr','').replace('        if(captured)*captured=header;\n','')
assert hashlib.sha256(unscoped.encode()).hexdigest()=="b2f66711c4dfd8e893506c0ea90182df7a20d9c04b482a4622d4b83781e9373d", 'Original pure Progress pointer reader changed'
(a.output/'progress_pointers_scoped.inc').write_text(pointers+';\n',encoding='utf-8')
(a.output/'progress_pointers_unscoped.inc').write_text(unscoped+';\n',encoding='utf-8')
# Both scopes end inside their pure reader blocks, before publication or any
# helper/CAS call. No other part of the mutation may retain page metadata.
selected=body(quest, 'const auto selected_pointer=')
assert selected.count('PageReadScope')==1 and 'compare_fixed_i32' not in selected and 'subscriber_lifecycle_append::' not in selected
remaining=quest_store.replace(census,'',1).replace(pointers,'',1).replace(selected,'',1)
assert remaining.count('PageReadScope')==1 and 'std::optional<apply_timing::PageReadScope> scalar_pages;' in remaining
assert remaining.index('scalar_pages.reset();')<remaining.index('if(!quest_restore_pause(')<remaining.rindex('quest_toon(')
assert quest_store.index(census)+len(census)<quest_store.index('quest_owners_=owners;quest_owners_ready_=true;')
for name in ['store_npc_state', 'store_npc_pose']:
    assert 'PageReadScope' not in body(quest, 'bool NativePort::' + name + '('), name
fresh=body(quest, 'bool quest_fresh_link(')
assert 'check.share_metadata&&check.scalar_owners&&link.portable_key.empty()&&!link.object' in fresh
assert 'PageReadScope pages(!pure_selected||!apply_timing::page_read_scope);' in fresh
assert 'PageReadScope pages(!share_metadata||!live_host_state::apply_timing::page_read_scope);' in body(saved, 'bool read_current(')
assert 'PageReadScope pages;' in body(owners, 'bool state_owners(')
# The pinned FindUID body only reads its lookup table. Keep the reader's page
# metadata across that lookup; mutation callbacks still have their own boundary.
owner_lookup = body(owners, 'bool state_owners_impl(')
saved_lookup = body(saved, 'bool read_current(std::vector<Current>& out, std::string& error, bool share_metadata)')
assert 'seh=lookup(api.find,op.local_uid,registered);' in owner_lookup
assert 'PageReadScope original_lookup' not in owner_lookup
assert 'code = find_call(api.find, uid, pointer);' in saved_lookup
assert 'PageReadScope original_lookup' not in saved_lookup
