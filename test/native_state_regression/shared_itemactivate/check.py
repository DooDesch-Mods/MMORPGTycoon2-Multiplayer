from pathlib import Path
import argparse
import hashlib
import json
import re

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, default=root / "candidate")
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
control = (args.source / "src/game/control_input/control_runtime.cpp").read_text(encoding="utf-8")
monster = (args.source / "src/game/monster_action_runtime.inc").read_text(encoding="utf-8")
original = (root / "baseline/src/game/monster_action_runtime.inc").read_text(encoding="utf-8")
shared = '''
        if(c.rva==0x207820){bool owned=false;
            if(!control_input::runtime::verify_item_activate(state.image,c.size,c.sha,owned,e))return false;
            if(owned)continue;
        }'''
dungeon_input_include = '#include "dungeon_input_runtime.inc"\n'
assert monster.count(dungeon_input_include) == 1
import_include = '#include "character_import_runtime.inc"\n'
assert monster.count('#include "ability_edit_runtime.inc"\n' + import_include + '#include "ability_membership_runtime.inc"\n') == 1
assert monster.count(import_include) == 1
assert hashlib.sha256((args.source / "src/game/character_import_runtime.inc").read_bytes()).hexdigest() == "27d37f92590ca35944e19ca81b753c6bb1befad1021dbce648f804f76d50e363", "Character Import admitted implementation differs"
cursor_shared = '        if(c.rva==0x5854e0){\n            if(!hooks::framework::verify_original_hash(state.image+c.rva,bytes.data(),bytes.size(),c.sha,e))\n                return fail(e,"Monster Cursor SetBehaviour original body differs RVA="+std::to_string(c.rva)+" original="+e);\n            continue;\n        }\n'
assert monster.count(cursor_shared) == 1
comparison = monster.replace(cursor_shared, '', 1).replace(import_include, '', 1)
for admitted in json.loads((root / "monster_edit_delta.json").read_text(encoding="utf-8")):
    assert comparison.count(admitted["after"]) == 1, "exact Monster edit delta differs"
    comparison = comparison.replace(admitted["after"], admitted["before"], 1)
comparison = comparison.replace(dungeon_input_include, '', 1)
assert comparison.count(shared)==1 and comparison.replace(shared,"")==original
route = control[control.index("bool verify_route("):control.index("bool routes(")]
provider_start = control.index("bool verify_item_activate(")
provider_end = control.find("bool verify_clock_time_scale(", provider_start)
if provider_end < 0:
    provider_end = control.index("bool capture_acquire(", provider_start)
provider = control[provider_start:provider_end]
assert "if(!verify_route(r,route,error))return false;" in control
base = args.source / "src/game"
pins = re.findall(r'\{0x207820,128,(?:5,)?"([0-9a-f]+)"\}',(base / "monster_action_codes.h").read_text()+(base / "control_input/control_route_initializers.inc").read_text())
assert len(pins)==2 and pins[0]==pins[1]=="f58f023d2a949f8307da8ba029e649767573240ea3395d832f2ebdd8dfa01b7c"
args.output.mkdir(parents=True,exist_ok=True)
(args.output / "control_itemactivate_extracted.inc").write_text(route+provider,encoding="utf-8")
code = monster[monster.index("bool code(Memory&"):monster.index("bool pin(Memory&")]
(args.output / "monster_itemactivate_extracted.inc").write_text(code,encoding="utf-8")
inputs = {"source":str(args.source),"original_shared_sha":pins[0],"bytes":128,
    "files":[{"path":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()} for path in (
        args.source / "src/game/control_input/control_runtime.cpp",args.source / "src/game/monster_action_runtime.inc",
        base / "release_ui_owner.h",root / "test_shared_itemactivate.cpp")]}
(args.output / "inputs.json").write_text(json.dumps(inputs,indent=2)+"\n",encoding="utf-8")
print("PASS shared pin equality, exact Dungeon input include admitted, unchanged Monster checks and shared production verifier reuse; fixture extracted")
