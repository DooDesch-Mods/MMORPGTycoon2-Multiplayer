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

files = {}
for row in json.loads((root / "baseline.json").read_text()):
    name = row["path"]
    before = (root / "baseline" / name).read_text()
    after = (args.source / name).read_text()
    assert hashlib.sha256((root / "baseline" / name).read_bytes()).hexdigest() == row["sha256"]
    files[name] = {"before": before, "after": after}

include = '#include "mt2mp/hooks/original_body.h"\n'
npc_after = files["src/game/npc_probe.cpp"]["after"]
refresh_include = '#include "npc_quest_info_refresh.h"\n'
assert npc_after.count(refresh_include) == 1
assert npc_after.count('#include "npc_quest_list_read.h"\n' + refresh_include) == 1
keyboard_include = '#include <SDL3/SDL_keyboard.h>\n'
assert npc_after.count(keyboard_include) == 1
assert npc_after.count('#include <SDL3/SDL_mouse.h>\n' + keyboard_include) == 1
npc_destination_include = '#include "npc_quest_npc_destination.h"\n'
assert npc_after.count(npc_destination_include) == 1
assert npc_after.count('#include "npc_quest_authoring_read.h"\n' + npc_destination_include) == 1
assert npc_after.replace(npc_destination_include, '', 1).replace(refresh_include, '', 1).replace(keyboard_include, '', 1) == include + files["src/game/npc_probe.cpp"]["before"]
for name, signature, output in (
    ("src/game/character_membership_runtime.inc", "bool code(", "character_extracted.inc"),
    ("src/game/dungeon_cells_runtime.inc", "bool code(", "cells_extracted.inc"),
    ("src/game/developer_staff/bug_policy_process.cpp", "bool code(", "policy_extracted.inc"),
    ("src/game/scenery_kiss/lifecycle_native.cpp", "bool code(", "scenery_extracted.inc"),
):
    before, after = files[name]["before"], files[name]["after"]
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
inputs = [args.source / name for name in files] + [args.source / "include/mt2mp/hooks/original_body.h", root / "test_original_body.cpp"]
(args.output / "inputs.json").write_text(json.dumps([{"path":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()} for path in inputs],indent=2)+"\n")
print("PASS exact changed function boundaries; original Confirm ownership/trampoline guard, Dungeon graph helpers and every unrelated byte pin unchanged")
