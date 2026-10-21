from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--build-source',type=Path);a=p.parse_args()
t=a.source.read_text(encoding='utf8');a.output.mkdir(parents=True,exist_ok=True)
def body(anchor,source=None):
 source=t if source is None else source
 start=source.index(anchor);pos=source.index('{',start);depth=1;end=pos+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
(a.output/'dungeon_original_scope.inc').write_text(body('    bool dungeon_scope(')+'\n',encoding='utf8')
for name,anchor,tail in [('operation','struct Operation {',';'),('build_operation','struct BuildOperation {',';'),('history_operation','struct HistoryOperation {',';'),('perform_place','action::Outcome perform_place(',''),('engine_jobs','bool construction_jobs(const action::Binding&',''),('runtime_jobs','bool construction_jobs(const action::Context&',''),('engine_bug_parent','bool bug_parent_alive(const action::Binding&',''),('runtime_bug_parent','bool bug_parent_alive(const action::Context&','')]:
 (a.output/f'construction_{name}.inc').write_text(body(anchor)+tail+'\n',encoding='utf8')

for name,anchor,tail in [('duplicate_type','struct DuplicateConstruction {',';'),
                         ('retire_absent','void retire_absent_construction(',''),
                         ('perform_duplicate','action::Outcome perform_duplicate(',''),
                         ('perform_duplicate_history','action::Outcome perform_duplicate_history(','')]:
 (a.output/f'construction_{name}.inc').write_text(body(anchor)+tail+'\n',encoding='utf8')

action_source=(a.source.parent/'typed_action/action_session.cpp').read_text(encoding='utf8')
duplicate_codec=(a.source.parent/'typed_action/duplicate_codec.inc').read_text(encoding='utf8')
duplicate_budget=(a.source.parent/'typed_action/duplicate_budget.inc').read_text(encoding='utf8')
(a.output/'construction_duplicate_valid.inc').write_text(body('bool uuid(',action_source)+'\n'+body('bool key(',action_source)+'\n'+
    body('bool duplicate_intent_valid(',duplicate_codec)+'\n'+body('bool valid_duplicate(',duplicate_budget)+'\n',encoding='utf8')

game=a.source.parent
producer=(a.build_source or game/'ui_build_runtime.cpp').read_text(encoding='utf8')
(a.output/'construction_scenery_place.inc').write_text(body('bool place_scenery(Runtime& r',producer)+'\n',encoding='utf8')
adapter=(game/'placement_kiss/placement_adapter.cpp').read_text(encoding='utf8')
(a.output/'construction_scenery_effect.inc').write_text(body('static bool reject(',adapter)+'\n'+body('bool valid_receipt(',adapter)+'\n',encoding='utf8')
contract=(game/'scenery_kiss/ui_contract.cpp').read_text(encoding='utf8')
state=(game/'scenery_kiss/scenery_state.cpp').read_text(encoding='utf8')
(a.output/'construction_scenery_guards.inc').write_text(body('bool fail(',state)+'\n'+body('bool valid_state(',state)+'\n'+body('bool valid_effect_counts(',contract)+'\n',encoding='utf8')
undo=(game/'object_identity/remove_runtime/copy/undo_witness.cpp').read_text(encoding='utf8')
(a.output/'construction_scenery_undo.inc').write_text(body('bool fail(',undo)+'\n'+body('bool validate_undo_commit(',undo)+'\n',encoding='utf8')
