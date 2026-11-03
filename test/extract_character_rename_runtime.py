from pathlib import Path
import argparse
import hashlib
import json

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
source = (a.source / 'src/game/character_rename_runtime.inc').read_text()
receipts = {}
for name, begin, end in [
    ('admission', 'bool ordered_mode()', 'bool reject('),
    ('proof', 'bool complete_proof(', 'bool sample('),
    ('presentation', 'bool presentation(', 'bool preview_presentation('),
    ('command_label', 'using TokenLabelEquals=', 'uint32_t native_command_label('),
    ('preview_public', 'bool preview_presentation(', 'bool pending_ui()'),
    ('capture', 'bool entry(void* receiver,void* record)try{', 'bool same_local('),
    ('tick', 'void tick(){', 'a::Outcome perform('),
    ('execution', 'a::Outcome perform(', '#include "character_rename_read.inc"'),
]:
    assert source.count(begin) == 1, begin
    body = source[source.index(begin):source.index(end, source.index(begin))]
    (a.output / f'character_rename_{name}_actual.inc').write_text(body)
    receipts[name] = hashlib.sha256(body.encode()).hexdigest()
preview = (a.source / 'src/game/character_rename_preview.inc').read_text()
for name, begin, end in [
    ('preview_types', 'using Attach=', 'struct Pin '),
    ('preview_context', 'bool current(', 'struct Scope '),
    ('preview_router', 'struct Scope ', 'bool actor('),
    ('preview_actor', 'bool actor(', 'bool enable('),
    ('preview_lifetime', 'bool bind(', '\n}\nbool preview_code'),
]:
    body = preview[preview.index(begin):preview.index(end, preview.index(begin))]
    (a.output / f'character_rename_{name}_actual.inc').write_text(body)
    receipts[name] = hashlib.sha256(body.encode()).hexdigest()
ui = (a.source / 'src/game/character_rename_ui.inc').read_text()
read = (a.source / 'src/game/character_rename_read.inc').read_text()
(a.output / 'character_rename_read_actual.inc').write_text(read)
receipts['read'] = hashlib.sha256(read.encode()).hexdigest()
opened = (a.source / 'src/game/character_rename_open.inc').read_text()
open_body = opened[:opened.index('bool open_code(')] + opened[opened.index('bool execute_open('):]
(a.output / 'character_rename_open.inc').write_text(open_body)
receipts['open'] = hashlib.sha256(open_body.encode()).hexdigest()
menu = (a.source / 'src/game/character_rename_menu.inc').read_text()
menu_body = menu[:menu.index('uint32_t activate_design(')] + menu[menu.index('bool execute_menu('):]
(a.output / 'character_rename_menu.inc').write_text(menu_body)
receipts['menu'] = hashlib.sha256(menu_body.encode()).hexdigest()
private_end = ui.index('\n}\nbool native_ui(')
for name, body in [('ui_helpers', ui[ui.index('struct UiJob {'):private_end]),
                   ('ui_public', ui[ui.index('bool native_ui('):])]:
    (a.output / f'character_rename_{name}_actual.inc').write_text(body)
    receipts[name] = hashlib.sha256(body.encode()).hexdigest()
view = (a.source / 'src/game/character_rename_view.inc').read_text()
for name, body in [('view_types', view[:view.index('uint32_t write_view_target(')]),
                   ('view', view[view.index('bool view_owner('):])]:
    (a.output / f'character_rename_{name}_actual.inc').write_text(body)
    receipts[name] = hashlib.sha256(body.encode()).hexdigest()
runtime = (a.source / 'src/game/typed_action_runtime.cpp').read_text()
begin = runtime.index('        if(action::tick_commands::authority::active()) {', runtime.index('    action::Outcome execute_action('))
end = runtime.index('        if(live_state_&&live_host_state::economic_fence()', begin)
body = runtime[begin:end]
(a.output / 'character_rename_engine_actual.inc').write_text(body)
receipts['engine'] = hashlib.sha256(body.encode()).hexdigest()
(a.output / 'character-rename-body-hashes.json').write_text(json.dumps(receipts, indent=2) + '\n')
