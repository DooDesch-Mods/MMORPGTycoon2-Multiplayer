from pathlib import Path
import argparse
import hashlib
import json

parser=argparse.ArgumentParser()
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
receipt={}
def save(name,text,source,scope):
    (args.output/name).write_text(text,encoding='utf-8',newline='\n')
    receipt[name]={'source':source,'scope':scope,'sha256':hashlib.sha256(text.encode()).hexdigest()}
def text(name):return (args.source/'src/game'/name).read_text()
helper=text('command_token_label.inc')
assert '0x96e1d0' in helper and 'entry_bytes=80' in helper
assert '8804bc96497105c4fc82d693ffc88533c3e6c1b9e70b92e3d310cdad72a0f2cc' in helper
save('command_token_label_actual.inc',helper,'src/game/command_token_label.inc','complete pin/read/native adapter')
save('command_token_label_adapter_actual.inc',helper[helper.index('using Equals='):helper.index('uint32_t native_read(')],
     'src/game/command_token_label.inc','complete portable GNU-string token adapter')
character=text('character_membership_runtime.inc')
save('character_variant_capture_actual.inc',character[character.index('bool new_token('):character.index('bool queue(')]+
     character[character.index('bool capture_variant('):character.index('bool pending_ui(')],
     'src/game/character_membership_runtime.inc','complete argument decoder and capture function')
ability=text('ability_membership_runtime.inc')
save('ability_new_capture_actual.inc',ability[ability.index('bool new_entry(void* receiver,void* record)try{'):ability.index('void remove_entry(void* receiver)try{')],
     'src/game/ability_membership_runtime.inc','complete NewAbility capture function')
for family,stop in [('dungeon_sound','    const auto controller=typed_action_runtime::controller_status();'),('dungeon_gizmo','    if(dungeon_input::preparing_gizmo_ui()){')]:
    source=family+'_runtime.inc'
    value=text(source)
    entry=value.index('bool entry(void* receiver,void* record)try{')
    cut=value.index(stop,entry)
    catch=value.index('}catch(const std::exception& exception)',cut)
    end=value.index('\n}\nbool pending_ui(',catch) if family=='dungeon_sound' else value.index('\nvoid reset_replay()',catch)
    prefix=value[entry:cut]
    tail=value[catch:end]
    save(family+'_capture_label_actual.inc',prefix+'    ++fixture_selected;return true;\n'+tail+'\n',
         'src/game/'+source,'exact admission prefix through original argument decoder; fixture selection counter replaces unchanged downstream ownership/effect work')
    replay=value.index('bool replay_record_equals(')
    save(family+'_replay_label_actual.inc',value[replay:entry],'src/game/'+source,'complete replay record label and argument confirmation')
    if family=='dungeon_gizmo':
        assert 'quest_list::label(api.label_equals,record,"DestroyGizmo",destroy)' in prefix
costume=text('costume_edit_runtime.inc')
assert 'quest_list::label(api.label_equals,record,"Apply",relevant)' in costume
assert 'command_token_label::read(memory,record,"WeaponCategory",weapon,error)' in costume
quest=(args.source/'src/game/npc_quest_list_runtime.inc').read_text()
assert 'uint32_t label(' in quest
diagnostic=(args.source/'src/game/ui_remove/diagnostic/process_calls.cpp').read_text()
assert 'api->label_equals(&record,api->destroy_label)' in diagnostic
(args.output/'body-hashes.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Extracted exact token adapter, two complete captures, two bounded capture prefixes and two complete replay confirmations; bare comparisons retained')
