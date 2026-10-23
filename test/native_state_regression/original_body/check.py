from pathlib import Path
import argparse
import hashlib
import json

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)

def function(text, start):
    at = text.index(start)
    body = text.index("{", at)
    depth = 1
    end = body + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[at:end]

import sys
sys.path.insert(0,str(args.source / "test"))
from scene_scenery_source_boundary import normalize
files = {}
for row in json.loads((root / "baseline.json").read_text()):
    name = row["path"]
    before = (root / "baseline" / name).read_text()
    after = (args.source / name).read_text()
    if name == "src/game/dungeon_cells_runtime.inc":
        from prepared_hook_batch_boundary import normalize_cells_enable
        after = normalize_cells_enable(after)
    after = normalize(name, after)
    assert hashlib.sha256((root / "baseline" / name).read_bytes()).hexdigest() == row["sha256"]
    files[name] = {"before": before, "after": after}

include = '#include "mt2mp/hooks/original_body.h"\n'
npc_after = files["src/game/npc_probe.cpp"]["after"]

building_sample_includes = '#include "dungeon_building_runtime.h"\n#include "dungeon_building_sample.h"\n#include "duplicate_history_value_calls.h"\n#include "building_npc_owner.h"\n#include "scenery_kiss/native_catalog.h"\n'
assert npc_after.count(building_sample_includes) == 1
assert npc_after.count('#include "dungeon_palette_runtime.h"\n' + building_sample_includes) == 1
npc_after = npc_after.replace(building_sample_includes, '', 1)
gizmo_live_includes = '#include "dungeon_gizmo_live_read.h"\n#include "dungeon_gizmo_live_codes.h"\n#include "live_host_state/network/node_native.h"\n#include "live_host_state/native_port.h"\n'
assert npc_after.count(gizmo_live_includes) == 1
assert npc_after.count('#include "dungeon_gizmo_record.h"\n' + gizmo_live_includes) == 1
npc_after = npc_after.replace(gizmo_live_includes, '', 1)
gizmo_control_includes = '#include "dungeon_gizmo_control_native.h"\n#include "dungeon_gizmo_control_calls.h"\n#include "dungeon_gizmo_move_read.h"\n'
assert npc_after.count(gizmo_control_includes) == 1
assert npc_after.count('#include "dungeon_input_capture.h"\n' + gizmo_control_includes) == 1
npc_after = npc_after.replace(gizmo_control_includes, '', 1)
npc_read_adapter = '    bool read(uintptr_t address,void* output,size_t size,std::string& error) {\n        return copy(address,output,size,error);\n    }\n'
assert npc_after.count(npc_read_adapter) == 1
assert npc_after.count('    host::ProcessMemory process;\n' + npc_read_adapter) == 1
npc_after = npc_after.replace(npc_read_adapter, '', 1)
refresh_include = '#include "npc_quest_info_refresh.h"\n'
assert npc_after.count(refresh_include) == 1
assert npc_after.count('#include "npc_quest_list_read.h"\n' + refresh_include) == 1
keyboard_include = '#include <SDL3/SDL_keyboard.h>\n'
assert npc_after.count(keyboard_include) == 1
assert npc_after.count('#include <SDL3/SDL_mouse.h>\n' + keyboard_include) == 1
npc_destination_include = '#include "npc_quest_npc_destination.h"\n'
assert npc_after.count(npc_destination_include) == 1
assert npc_after.count('#include "npc_quest_authoring_read.h"\n' + npc_destination_include) == 1
dungeon_input_includes = '#include "dungeon_input_runtime.h"\n#include "dungeon_input_codes.h"\n#include "dungeon_input_calls.h"\n#include "dungeon_input_native.h"\n#include "dungeon_input_action_calls.h"\n#include "dungeon_input_action_codes.h"\n#include "dungeon_input_capture.h"\n#include "dungeon_fill_native.h"\n'
assert npc_after.count(dungeon_input_includes) == 1
assert npc_after.count(dungeon_input_includes + '#include "dungeon_cells_calls.h"\n') == 1
monster_history_includes = '#include "intervention_census.h"\n#include "monster_history_read.h"\n#include "monster_history_codes.h"\n#include "monster_move_destination.h"\n'
assert npc_after.count(monster_history_includes) == 1
assert npc_after.count('#include "monster_action_runtime.h"\n' + monster_history_includes) == 1
zone_headers = '#include "zone_edit_runtime.h"\n#include "world_zone_edit.h"\n#include "zone_edit_read.h"\n#include "zone_edit_stored.h"\n#include "zone_edit_calls.h"\n#include "dungeon_cell_graph.h"\n#include "quest_cascade_read.h"\n#include "duplicate_quest_history.h"\n#include "zone_quest_history.h"\n#include "group_reader.h"\n#include "duplicate_calls.h"\n#include "ui_remove_runtime.h"\n#include "npc_quest_building_key.h"\n#include <charconv>\n'
zone_leaves = '#include "zone_edit_sample.inc"\n#include "zone_edit_capture.inc"\n#include "zone_edit_graph.inc"\n#include "zone_edit_execute.inc"\n#include "zone_edit_remove.inc"\n#include "zone_edit_history.inc"\n#include "zone_edit_routes.inc"\n'
assert npc_after.startswith(zone_headers) and npc_after.count(zone_headers) == 1
assert npc_after.count('#include "character_actor_records_runtime.inc"\n' + zone_leaves) == 1
npc_after = npc_after.removeprefix(zone_headers).replace(zone_leaves, '', 1)
import_includes = '#include "character_import_runtime.h"\n#include "character_import_native.h"\n#include "character_import_view.h"\n#include "general_project_runtime.h"\n'
assert npc_after.count(import_includes) == 1
assert npc_after.count('#include "character_membership_runtime.h"\n' + import_includes) == 1
npc_after = npc_after.replace(import_includes, '', 1)
adjustment_include = '#include "ability_adjustment_view.h"\n'
assert npc_after.count(adjustment_include) == 1
assert npc_after.count('#include "costume_weapon_read.h"\n' + adjustment_include) == 1
npc_after = npc_after.replace(adjustment_include, '', 1)
costume_weapon_include = '#include "costume_weapon_read.h"\n'
assert npc_after.count(costume_weapon_include) == 1
assert npc_after.count('#include "costume_record_reader.h"\n' + costume_weapon_include) == 1
npc_after = npc_after.replace(costume_weapon_include, '', 1)
npc_history_includes = '#include "npc_history_graph.h"\n#include "npc_history_quests.h"\n#include "npc_history_budget.h"\n'
assert npc_after.count(npc_history_includes) == 1
assert npc_after.count('#include "npc_probe.h"\n' + npc_history_includes) == 1
npc_after = npc_after.replace(npc_history_includes, '', 1)
assert npc_after.replace(monster_history_includes, '', 1).replace(dungeon_input_includes, '', 1).replace(npc_destination_include, '', 1).replace(refresh_include, '', 1).replace(keyboard_include, '', 1) == include + files["src/game/npc_probe.cpp"]["before"]
for name, signature, output in (
    ("src/game/character_membership_runtime.inc", "bool code(", "character_extracted.inc"),
    ("src/game/dungeon_cells_runtime.inc", "bool code(", "cells_extracted.inc"),
    ("src/game/developer_staff/bug_policy_process.cpp", "bool code(", "policy_extracted.inc"),
    ("src/game/scenery_kiss/lifecycle_native.cpp", "bool code(", "scenery_extracted.inc"),
):
    before, after = files[name]["before"], files[name]["after"]
    if "character_membership" in name:
        import_forward = 'a::Outcome perform_import(Memory&,const a::Binding&,const a::Request&,const a::Evidence*,const Sample&,std::string&);\n'
        assert after.count('void confirm_entry(void*);\n' + import_forward) == 1
        after = after.replace(import_forward, '', 1)
        for old_import, new_import in (
            ('Memory memory;if(!code(memory,error))return false;operations.clear();local_views.clear();return true;',
             'Memory memory;if(!code(memory,error)||!enable_import(error))return false;operations.clear();local_views.clear();return true;'),
            ('    if(!code(memory,error))return false;ready=true;return true;',
             '    if(!code(memory,error)||!enable_import(error))return false;ready=true;return true;'),
        ):
            assert after.count(new_import) == 1
            after = after.replace(new_import, old_import, 1)
        import_dispatch = '    if(intent.command==a::CharacterMembershipCommand::Import)return perform_import(memory,binding,request,authority,before,error);\n'
        assert after.count(import_dispatch) == 1
        after = after.replace(import_dispatch, '', 1)
        import_status = '{"operations",rows},{"import_input",import_status()}'
        assert after.count(import_status) == 1
        after = after.replace(import_status, '{"operations",rows}', 1)
    old, new = function(before, signature), function(after, signature)
    assert after.replace(new, old).removeprefix(include) == before
    (args.output / output).write_text(new + "\n", encoding="utf-8")
    if "character_membership" in name:
        guard_end = old.index("        if(mt2mp::sha256(bytes)==pin.sha)")
        assert new[:guard_end] == old[:guard_end], "exact owned Confirm guard must remain unchanged"
    if "dungeon_cells" in name:
        assert new[new.index("    for(const auto& pin:dungeon_cells_codes::graph_helpers)"):] == old[old.index("    for(const auto& pin:dungeon_cells_codes::graph_helpers)"):]

duplicate = files["src/game/duplicate_codeguards.h"]
old = 'if(std::memcmp(actual.data(),guard.bytes,guard.size)){error="Duplicate original helper differs RVA="+std::to_string(guard.rva);return false;}'
new = 'if(!hooks::framework::verify_original_bytes(image+guard.rva,actual.data(),guard.bytes,guard.size,error)){error="Duplicate original helper differs RVA="+std::to_string(guard.rva)+" original="+error;return false;}'
assert duplicate["after"].removeprefix(include).replace(new, old) == duplicate["before"]
table = duplicate["before"][duplicate["before"].index("    struct Guard{"):duplicate["before"].index("    for(const auto& guard:guards)")]
(args.output / "duplicate_fixture_guards.inc").write_text(table + '\n    for(const auto& guard:guards)memory[image+guard.rva]=std::vector<unsigned char>(guard.bytes,guard.bytes+guard.size);\n')
monster = (args.source / "src/game/monster_action_runtime.inc").read_text(encoding="utf-8")
(args.output / "monster_cursor_extracted.inc").write_text(function(monster,"bool code(Memory&")+"\n",encoding="utf-8")
import re
npc_history = function((args.source / 'src/game/npc_history_runtime.inc').read_text(encoding='utf-8'), 'bool code(Memory&')
npc_pins = re.findall(r'\{(0x[0-9a-f]+),(\d+),"([0-9a-f]{64})"\}', npc_history)
assert len(npc_pins) == 6 and npc_pins[4] == ('0x55ed80', '704', '555aef77c8b022b696a31e5c65a0c6c8a8f860449323943d4980e48187214aee')
for index, (_, _, digest) in enumerate(npc_pins):
    assert npc_history.count('"' + digest + '"') == 1
    npc_history = npc_history.replace('"' + digest + '"', 'npc_history_hashes[' + str(index) + '].c_str()', 1)
(args.output / 'npc_history_extracted.inc').write_text(npc_history + '\n', encoding='utf-8')
(args.output / 'npc_history_fixture_pins.inc').write_text('const std::array<std::pair<uintptr_t,size_t>,6> npc_history_pins{{' + ','.join('{' + rva + ',' + size + '}' for rva,size,_ in npc_pins) + '}};\n', encoding='utf-8')
inputs = [args.source / "src/game/monster_action_runtime.inc"] + [args.source / name for name in files] + [args.source / "include/mt2mp/hooks/original_body.h", root / "test_original_body.cpp"]
inputs.append(args.source / 'src/game/npc_history_runtime.inc')
from gizmo_code_digest_boundary import normalize_gizmo_code
gizmo_text = (args.source / 'src/game/dungeon_gizmo_runtime.inc').read_text(encoding='utf-8')
gizmo_baseline = normalize_gizmo_code(gizmo_text)
(args.output / 'gizmo_code_actual.inc').write_text(function(gizmo_text, 'bool code(') + '\n', encoding='utf-8')
(args.output / 'gizmo_code_baseline.inc').write_text(function(gizmo_baseline, 'bool code(').replace('bool code(', 'bool code_baseline(', 1) + '\n', encoding='utf-8')
inputs.extend([args.source / 'src/game/dungeon_gizmo_runtime.inc', args.source / 'include/mt2mp/hooks/code_body_digest.h', args.source / 'test/gizmo_code_digest_boundary.py'])
(args.output / "inputs.json").write_text(json.dumps([{"path":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()} for path in inputs],indent=2)+"\n")
print("PASS exact changed function boundaries; original Confirm ownership/trampoline guard, Dungeon graph helpers and every unrelated byte pin unchanged")
