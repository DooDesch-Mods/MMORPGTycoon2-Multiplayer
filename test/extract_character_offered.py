from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);receipts={}
def save(name,body):
    (a.output/name).write_text(body,encoding='utf-8');receipts[name]=hashlib.sha256(body.encode()).hexdigest()
opened=(a.source/'src/game/character_rename_open.inc').read_text(encoding='utf-8')
save('character_open_array_actual.inc',opened[opened.index('bool open_array('):opened.index('#include "character_rename_menu.inc"')])
source=(a.source/'src/game/character_offered_ui.inc').read_text(encoding='utf-8')
start=source.index('bool execute_edit_ui(');end=source.index('\n}\n}\nstd::string native_edit_input_token()',start)+2
save('character_offered_costume_driver_actual.inc',source[start:end]+'\n')
save('character_offered_page_actual.inc',source[source.index('bool edit_page('):source.index('bool costume_offered(')])
save('character_offered_public_actual.inc',source[source.index('std::string native_edit_input_token()'):source.rindex('\n}')])
source=(a.source/'src/game/character_offered_ability.inc').read_text(encoding='utf-8')
save('character_offered_ability_driver_actual.inc',source[source.index('bool execute_ability_ui('):])
save('character_offered_control_state_actual.inc',source[source.index('bool ability_controls('):source.index('bool execute_ability_ui(')])
save('character_offered_icons_actual.inc',source[source.index('struct OfferedAbility '):source.index('bool ability_controls(')])
(a.output/'character-offered-body-hashes.json').write_text(json.dumps(receipts,indent=2)+'\n')
print('Extracted complete actual shared Costume/Ability input drivers and public queue API')
