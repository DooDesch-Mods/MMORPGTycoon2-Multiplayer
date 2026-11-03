from pathlib import Path
import argparse
import hashlib
import json

p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=(a.source/'src/game/ability_edit_runtime.inc').read_text()
adjustment=(a.source/'src/game/ability_adjustment_runtime.inc').read_text()
parent=(a.source/'src/game/character_rename_runtime.inc').read_text()
receipt={}
def emit(name,text,origin):
    (a.output/name).write_text(text,encoding='utf-8',newline='\n')
    receipt[name]={'source':origin,'sha256':hashlib.sha256(text.encode()).hexdigest()}
def function(text,anchor):
    start=text.index(anchor);at=text.index('{',start)+1;depth=1
    while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[start:at]+'\n'
emit('ability_ordered_helpers_actual.inc',source[source.index('bool ordered_mode()'):source.index('#include "ability_adjustment_runtime.inc"')],'src/game/ability_edit_runtime.inc')
emit('ability_same_local_actual.inc',source[source.index('bool same_local('):source.index('bool ordered_mode()')],'src/game/ability_edit_runtime.inc')
emit('ability_record_actual.inc',function(source,'json record_json('),'src/game/ability_edit_runtime.inc')
emit('ability_tick_actual.inc',source[source.index('void tick(){'):source.index('a::Outcome perform(')],'src/game/ability_edit_runtime.inc')
emit('ability_perform_actual.inc',source[source.index('a::Outcome perform('):source.index('bool observe(')],'src/game/ability_edit_runtime.inc')
emit('ability_observe_actual.inc',source[source.index('bool observe('):source.index('json status()')],'src/game/ability_edit_runtime.inc')
emit('ability_capture_actual.inc',adjustment[adjustment.index('bool capture_adjustment_impl('):adjustment.index('a::Outcome perform_adjustment(')],'src/game/ability_adjustment_runtime.inc')
emit('ability_adjustment_calls_actual.inc',adjustment[adjustment.index('struct AdjustmentCalls {'):adjustment.index('bool capture_adjustment_impl(')],'src/game/ability_adjustment_runtime.inc')
emit('ability_adjustment_perform_actual.inc',adjustment[adjustment.index('a::Outcome perform_adjustment('):],'src/game/ability_adjustment_runtime.inc')
emit('ability_view_tail_actual.inc',adjustment[adjustment.index('void view_select_entry('):adjustment.index('bool view_route(')],'src/game/ability_adjustment_runtime.inc')
emit('ability_parent_capture_binding_actual.inc',function(parent,'bool capture_binding('),'src/game/character_rename_runtime.inc')
emit('ability_types_actual.inc',function(source,'struct Pending {').removesuffix('\n')+';\n'+function(source,'struct Operation {').removesuffix('\n')+';\n','src/game/ability_edit_runtime.inc')
(a.output/'body-hashes.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Extracted complete Ability adjustment capture, Context tick, consumed executor, effect/protected checks, presentation calls and observation/retirement bodies')
