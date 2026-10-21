from pathlib import Path
import argparse
import hashlib
import json
import re

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
quest = (args.source / "src/game/npc_quest_list_runtime.inc").read_text()
palette = (args.source / "src/game/dungeon_palette_runtime.inc").read_text()
old_quest = (root / "baseline/src/game/npc_quest_list_runtime.inc").read_text()
old_palette = (root / "baseline/src/game/dungeon_palette_runtime.inc").read_text()
helper_start = quest.index("bool checkbox_original(")
inner_start = quest.index("bool inner_codes(")
inner_end = quest.index("guards::Owned owned(")
original_start = old_quest.index("bool inner_codes(")
original_end = old_quest.index("bool read_route(")
# Admit only the fixed, separately checked presentation refresh. The remaining
# historical two-consumer delta is still compared in full against its baseline.
presentation = [('    npc_quest_info_refresh::Receipt info_refresh;\n', ''), ('#include "npc_quest_info_refresh_runtime.inc"\n', ''), ('    op.retired=true;refresh_info(op);return true;', '    op.retired=true;return true;'), ('{"info_refresh",info_refresh_status(op.info_refresh)},', '')]
comparison = quest[:helper_start] + quest[inner_end:]
for admitted, previous in presentation:
    assert comparison.count(admitted) == 1, f"Quest Info admitted hunk differs: {admitted!r}"
    comparison = comparison.replace(admitted, previous, 1)
assert comparison == old_quest[:original_start] + old_quest[original_end:]
presentation_bodies = {'npc_quest_info_refresh.h': '604ddf6c57387285f8c8eb5937bd8c88cb1a954419749ffbf33f9fdf0539f825', 'npc_quest_info_refresh_runtime.inc': 'a0092b80e9c4302b4659e63d7b7aa26f78bd158a989c6d1d9bcc4123c99f9888'}
for name, expected in presentation_bodies.items():
    body = (args.source / "src/game" / name).read_text(encoding="utf-8")
    assert hashlib.sha256(body.encode("utf-8")).hexdigest() == expected, f"Quest Info presentation body differs: {name}"
presentation_pins = 'inline constexpr Code info_refresh[]{\n    {0x758d90,2454,"8a9db1f7542c441879feaeaf7cc363d0ebd13b02ddb51c13b82dde924ea7e003"},\n    {0x785290,132,"7b0e9753f9652e8f6375a1fdeede5f8fd0c52204a87e000e5c952e8ef949414f"},\n    {0x882400,22,"b5bc53da3a843f973fbcb98bb1822f7d3155d28c6aba68a8a1bec18032742626"},\n    {0xa1ab30,512,"08e2bb77467685d89095838882d33a865532f193254ccaa63952aaf386095b72"},\n};\n'
assert (args.source / "src/game/npc_quest_list_codes.h").read_text().count(presentation_pins) == 1
inner = quest[inner_start:inner_end]
restored = inner.replace('if(!m.copy(state.image+code.rva,bytes.data(),bytes.size(),e))', 'if(!m.copy(state.image+code.rva,bytes.data(),bytes.size(),e)||!quest_bridge::sha256(bytes,sha,e))')
restored = restored.replace('        if(code.rva==0x6a1780&&!checkbox_original(state.image+code.rva,bytes,e))return false;\n        if(!quest_bridge::sha256(bytes,sha,e))return false;\n', '')
assert restored == old_quest[original_start:original_end]
added = 'if(pin.rva==0x6a1780&&!quest_list::checkbox_original(state.image+pin.rva,bytes,error))return false;'
# The additional District color route changes only authentication of the same
# complete helper extent. Retain the original guard and compare every other byte.
district_guard = 'if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))return fail(error,"Dungeon palette original explicit-owner helper body differs RVA="+std::to_string(pin.rva)+": "+error);'
prior_guard = 'if(!quest_bridge::sha256(bytes,hash,error))return false;if(hash!=pin.sha)return fail(error,"Dungeon palette original explicit-owner helper body differs RVA="+std::to_string(pin.rva));'
assert palette.count(district_guard) == 1, "District palette full-body guard missing"
palette_comparison = palette.replace(district_guard, prior_guard, 1)
# The D01 sample is an adjacent shared-reader include only.
building_join = '#include "dungeon_authored_sample.inc"\n#include "dungeon_building_sample.inc"\n'
assert palette_comparison.count(building_join) == 1, 'Dungeon Building sample include seam differs'
palette_comparison = palette_comparison.replace(building_join, '#include "dungeon_authored_sample.inc"\n', 1)
# The authored Cells reader is shared with Action66 at this fixed include seam.
# Normalize only that include; retain the full historical palette comparison.
authored_join = '#include "dungeon_cells_graph.inc"\n#include "dungeon_authored_sample.inc"\n#include "dungeon_cells_port.inc"'
previous_join = '#include "dungeon_cells_graph.inc"\n#include "dungeon_cells_port.inc"'
assert palette_comparison.count(authored_join) == 1, "Dungeon authored reader include seam differs"
palette_comparison = palette_comparison.replace(authored_join, previous_join, 1)
assert palette_comparison.replace(added, '').replace('if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;if(!quest_bridge::sha256(bytes,hash,error))', 'if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))') == old_palette
dependencies = args.source
base = args.source / "src/game"
if not (base / "gm_policy_route_pins.inc").exists():
    base = dependencies / "src/game"
routes = {int(x, 16) for x in re.findall(r'\{0x([0-9a-f]+),', (base / "gm_policy_route_pins.inc").read_text())}
overlaps = {}
for name in ("npc_quest_authoring_codes.h", "npc_quest_list_codes.h", "npc_quest_list_ui_codes.h", "dungeon_palette_codes.h"):
    text = (base / name).read_text()
    overlaps[name] = sorted({int(x, 0) for x in re.findall(r'\{(0x[0-9a-f]+|[0-9]+),', text)} & routes)
assert overlaps == {"npc_quest_authoring_codes.h": [], "npc_quest_list_codes.h": [0x6a1780], "npc_quest_list_ui_codes.h": [], "dungeon_palette_codes.h": [0x6a1780]}
assert '{0x6a1780,153,"b788c070ee48b5a25c3b5c7034e2afa585343d63ef447255117ec01598329367"}' in (base / "npc_quest_list_codes.h").read_text()
assert '{6952832,153,"b788c070ee48b5a25c3b5c7034e2afa585343d63ef447255117ec01598329367"}' in (base / "dungeon_palette_codes.h").read_text()
args.output.mkdir(parents=True, exist_ok=True)
(args.output / "quest_extracted.inc").write_text(quest[quest.index("bool read_route("):inner_end], encoding="utf-8")
(args.output / "palette_extracted.inc").write_text(palette[palette.index("bool code("):palette.index("bool sample(")], encoding="utf-8")
backend = args.source / "src/hooks/framework_backend.cpp"
if not backend.exists():
    backend = dependencies / "src/hooks/framework_backend.cpp"
files = [args.source / "src/game/npc_quest_list_runtime.inc", args.source / "src/game/dungeon_palette_runtime.inc", backend, root / "test_shared_quest.cpp"]
(args.output / "inputs.json").write_text(json.dumps({"overlaps": overlaps, "files": [{"path": str(p), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]}, indent=2) + "\n")
print("PASS exact two-consumer delta plus pinned Info refresh; other bodies unchanged; eight GMPolicy routes checked against four related pin tables; extracted actual consumer bodies")
