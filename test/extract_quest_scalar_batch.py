from pathlib import Path
import argparse
import hashlib
p=argparse.ArgumentParser()
p.add_argument('--root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline',type=Path)
p.add_argument('--dependencies',type=Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=(a.root/'src/game/live_host_state/day_rollover/economy/native_quest_state.inc').read_text()
def body(text,anchor):
    start=text.index(anchor)
    signature=start
    if not anchor.startswith('struct '):
        signature=text.index('(',start)+1;parens=1
        while parens:
            parens+=(text[signature]=='(')-(text[signature]==')');signature+=1
    end=text.index('{',signature)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
for name,anchor in [('noop_weak_struct','struct QuestNoopWeak {'),('noop_row_struct','struct QuestNoopRow {'),('noop_weak','bool quest_noop_weak('),('noop_row','bool quest_noop_row('),('fresh_struct','struct QuestFresh {'),('fresh','bool quest_fresh_link('),('pause','bool quest_restore_pause('),('phase','bool NativePort::with_quest_stores('),('begin','bool NativePort::begin_quest_stores('),('admit_history','bool NativePort::admit_quest_history('),('clear','void NativePort::clear_quest_span('),('end','void NativePort::end_quest_stores('),('store','bool NativePort::store_quest_state(')]:
    (a.output/(name+'.inc')).write_text(body(source,anchor)+(';' if name.endswith('_struct') else '')+'\n')
# Exercise the actual RAII scope rather than a second hand-written scope.
monetary=(a.root/'src/game/live_host_state/day_rollover/economy/monetary.cpp').read_text()
line=next(line.strip() for line in monetary.splitlines() if 'struct QuestScope ' in line)
(a.output/'scope.inc').write_text(line+'\n')
assert monetary.index('} quest_scope{port};') < monetary.index('port.store_quest_state(') < monetary.index('apply_timing::Part::NpcStores')
# A retained span never changes the callback-capable Progress and weapon bodies.
if a.baseline:
    base=a.baseline.read_text()
    for start,end in [('    const auto refresh_weapon=', '    if(actual==target)'),('    const auto weapon_fence=', '    // A freshly captured equal Progress list')]:
        assert source[source.index(start):source.index(end)]==base[base.index(start):base.index(end)]
print('Extracted actual Quest store, fresh owner, pause and scope bodies')
runtime=((a.dependencies or a.root)/'src/game/subscriber_lifecycle/runtime_append.cpp').read_text()
weak=body(source,'bool quest_noop_weak(').replace('bool quest_noop_weak(const Pin& pin,uintptr_t instance,QuestNoopWeak& out','bool quest_weak(uintptr_t instance,QuestWeak& out').replace('pin.image','image_base')
row=body(source,'bool quest_noop_row(').replace('bool quest_noop_row(const Pin& pin,uintptr_t p,QuestNoopRow& out','bool progress_row(uintptr_t p,ProgressRow& out').replace('pin.image','image_base').replace('quest_noop_weak(pin,p+0x58,out.quest,e)','quest_weak(p+0x58,out.quest,e)').replace('\n    apply_timing::PageReadScope pages;','')
assert weak==body(runtime,'bool quest_weak('), 'Selected Quest weak proof drifted from original predicates/errors'
assert row==body(runtime,'bool progress_row('), 'Selected Progress row proof drifted from original predicates/errors'

# The full reader still guards initial/final/expired state and every callback
# boundary. Only the new per-row repeat is conditional; selected proofs remain.
store=body(source,'bool NativePort::store_quest_state(')
closed_store=store.replace('    if(quest_batch_){\n        if(!subscriber_lifecycle_append::with_quest_edit_row(*quest_batch_,edits,exit_edits,e))return false;\n    }else if(!subscriber_lifecycle_append::with_quest_edits(world.uuid,frame,edits,exit_edits,edit_scope,e))return false;', '    if(!subscriber_lifecycle_append::with_quest_edits(world.uuid,frame,edits,exit_edits,edit_scope,e))return false;',1)
closed_store=closed_store.replace('    if(quest_batch_&&subscriber_lifecycle_append::quest_edits_held(*quest_batch_)){\n        if(!subscriber_lifecycle_append::verify_quest_edits_held(*quest_batch_,e))return false;\n    }else if(!quest_restore_pause(p,e,[&](){clear_quest_span();}))return false;', '    if(!quest_restore_pause(p,e,[&](){clear_quest_span();}))return false;',1)
assert hashlib.sha256(closed_store.encode()).hexdigest()=='1335740a19a4b884fa3eb7fcff22e919eae34d61526999cb9ab68d6d39fe9418', 'Standalone store body changed outside phase dispatch'
metadata_previous=closed_store.replace('    // Equal links and weapon keep this row callback-free until pause restoration.\n    // Share page metadata only; every owner and value is still read live.\n    std::optional<apply_timing::PageReadScope> scalar_pages;\n    if(scalar_only)scalar_pages.emplace();\n','',1)
metadata_previous=metadata_previous.replace(';fresh.share_metadata=true;', ';',1)
metadata_previous=metadata_previous.replace('apply_timing::PageReadScope pointer_pages(!scalar_only||!apply_timing::page_read_scope);', 'apply_timing::PageReadScope pointer_pages;')
metadata_previous=metadata_previous.replace('    scalar_pages.reset();\n','',1)
assert 'scalar_pages' not in metadata_previous and metadata_previous != store
assert hashlib.sha256(metadata_previous.encode()).hexdigest()=='85f68d385d5c693bcdcc28b8b6fff13c4295c0c57d8ed2f604f223af35b29cd3', 'Pre-metadata Quest store reconstruction changed'
(a.output/'store_metadata_previous.inc').write_text(metadata_previous.replace('NativePort::store_quest_state(', 'NativePort::store_quest_state_metadata_previous(',1)+'\n')
assert store.count('quest_toon(')==4
assert 'const bool validate_after_callback=!owners_current;' in store
assert 'if(validate_after_callback){\n            std::optional<quest_state::Toon> live;' in store
assert 'else if(!pointers(rows))return false;' in store
assert 'quest_noop_row(p,rows[row_index],row,e)' in store
assert 'observed.npc' not in store and 'observed.quest' not in store
reader=((a.dependencies or a.root)/'src/game/npc_quest_visitor_reader.h').read_text()
for guard in ['before.capacity>100000','before.data>UINTPTR_MAX-size_t(before.count)*sizeof(uintptr_t)', '!progress_objects.insert(progress).second', 'before.capacity!=after.capacity', 'pointer!=pointers[size_t(i)]']:
    assert guard in reader,guard

# Retain the immediate prior body for exact membership workload comparison.
assert store.count('fresh.instance_member=!scalar_only;')==1
previous=metadata_previous.replace('fresh.instance_member=!scalar_only;', 'fresh.instance_member=true;')
(a.output/'store_membership_previous.inc').write_text(previous.replace('NativePort::store_quest_state(', 'NativePort::store_quest_state_membership_previous(',1)+'\n')
# Reconstruct the historical body after restoring the prior membership guard.
previous=previous.replace('    if(apply_timing::quest_store_depth)apply_timing::record(scalar_only?apply_timing::Part::QuestScalarQualified:apply_timing::Part::QuestNonqualified,0);\n', '')
original=previous.replace('            if(!admit_quest_history(before,e))return false;\n','').replace('const auto pointers=[&](std::vector<uintptr_t>& out,npc_quest_visitor_reader::ArrayHeader* captured=nullptr){','const auto pointers=[&](std::vector<uintptr_t>& out){')
original=original.replace('        if(captured)*captured=header;\n','')
original=original.replace('    npc_quest_visitor_reader::ArrayHeader scalar_header;\n','').replace('pointers(rows,&scalar_header)','pointers(rows)')
start=original.index('    // Only scalar stores run between this ordered capture')
end=original.index('    bool owners_current=',start)
original=original[:start]+original[end:]
original=original.replace('scalar_only?selected_pointer(i):pointers(rows)','pointers(rows)').replace('if(!(pointers(rows))||','if(!pointers(rows)||')
original=original.replace('    const auto edits=[&](subscriber_lifecycle_append::QuestEditBatch& batch,std::string& error){\n    (void)error;\n','')
original=original.replace('    return true;\n    };\n    mt2mp::subscriber_lifecycle::SaveScopeReceipt edit_scope;\n    const auto exit_edits=[&](std::string& error){\n        fresh.instance_member=false;\n        return quest_fresh_link(&fresh,{},error);\n    };\n    if(!subscriber_lifecycle_append::with_quest_edits(world.uuid,frame,edits,exit_edits,edit_scope,e))return false;\n','')
for helper in ('retire_progress','ensure_progress','bind_quest_instance'):
    original=original.replace('subscriber_lifecycle_append::'+helper+'(batch,request,','subscriber_lifecycle_append::'+helper+'(request,')
assert hashlib.sha256(original.encode()).hexdigest()=='88aeaac2f1679c3786b8cdeef04f90391fe971400bbb2d1838d3d50cf730756a', 'Original Quest store reconstruction changed'
(a.output/'store_original.inc').write_text(original.replace('NativePort::store_quest_state(', 'NativePort::store_quest_state_original(',1)+'\n')
assert store.count('scalar_only?selected_pointer(i):pointers(rows)')==1
assert store.index('selected_pointer(i)') < store.index('progress=rows[i];fresh.progress=progress;')

# Compile the actual current-subscriber branch with instrumented boundary ports.
native=(a.root/'src/game/live_host_state/day_rollover/economy/native_monetary.cpp').read_text()
current=body(native,'bool NativePort::current_subscriber(')
(a.output/'current_subscriber.inc').write_text(current.replace('NativePort::current_subscriber(', 'NativePort::current_subscriber_checked(',1)+'\n')
