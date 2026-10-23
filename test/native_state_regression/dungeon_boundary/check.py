from pathlib import Path
import argparse
import hashlib
import json
import re
import base64

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--framework', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--overlay', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parent

import sys
sys.path.insert(0, str(args.source / 'test'))
from scene_scenery_source_boundary import normalize

changes = (
    (b'array<unsigned char,20> trampoline', b'array<unsigned char,19> trampoline'),
    (b'trampoline.data()+12,8', b'trampoline.data()+11,8'),
    (b'ui_code.prefix.begin()+6', b'ui_code.prefix.begin()+5'),
    (b'trampoline.begin()+6', b'trampoline.begin()+5'),
    (b'resume!=state.image+ui_code.rva+6', b'resume!=state.image+ui_code.rva+5'),
    (b'original six-byte instruction', b'original five-byte instruction'),
)
helper_changes = {
    'dungeon_light_runtime.inc': (
        b"for(const auto& pin:dungeon_light_codes::helpers){bytes.assign(pin.size,'\\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;\n        if(hash!=pin.sha)return fail(error,\"Dungeon light original preparation/ownership helper body differs RVA=\"+std::to_string(pin.rva));}",
        b"for(const auto& pin:dungeon_light_codes::helpers){bytes.assign(pin.size,'\\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;\n        if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))return fail(error,\"Dungeon light original preparation/ownership helper body differs RVA=\"+std::to_string(pin.rva)+\": \"+error);}",
    ),
    'dungeon_palette_runtime.inc': (
        b'if(!quest_bridge::sha256(bytes,hash,error))return false;if(hash!=pin.sha)return fail(error,"Dungeon palette original explicit-owner helper body differs RVA="+std::to_string(pin.rva));',
        b'if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))return fail(error,"Dungeon palette original explicit-owner helper body differs RVA="+std::to_string(pin.rva)+": "+error);',
    ),
}
texts = {}
inputs = []
for row in json.loads((root / 'baseline.json').read_text()):
    name = row['path']
    path = args.source / name
    if args.overlay and (args.overlay / name).exists():
        path = args.overlay / name
    data = path.read_bytes()
    original = data
    if name == 'src/game/dungeon_cells_runtime.inc':
        # Reverse only the exact Scene Scenery addition, then recover the
        # original all-CRLF bytes required by the unchanged historical hash.
        from prepared_hook_batch_boundary import normalize_cells_enable
        original = normalize(name, normalize_cells_enable(original.decode('utf-8'))).encode('utf-8')
        assert b'\r' not in original, 'Scene Scenery prior Cells newline form differs'
        original = original.replace(b'\n', b'\r\n')
    if name.endswith('dungeon_gizmo_runtime.inc'):
        from gizmo_code_digest_boundary import normalize_gizmo_code
        original = normalize_gizmo_code(original.decode('utf-8')).encode('utf-8')
        rename_include = b'#include "dungeon_gizmo_live_native.inc"\n#include "dungeon_gizmo_rename.inc"\n'
        if rename_include in original:
            assert original.count(rename_include) == 1, 'Gizmo Rename include is not unique'
            original = original.replace(rename_include, b'#include "dungeon_gizmo_live_native.inc"\n')
        delta = json.loads((root / 'd03_gizmo_delta.json').read_text())
        assert hashlib.sha256(original).hexdigest() == delta['after_sha256'], 'D03 Gizmo runtime differs from its exact admitted candidate'
        for change in reversed(delta['changes']):
            start, end = change['start'], change['end']
            assert original[start:end] == base64.b64decode(change['after']), 'D03 Gizmo exact bounded source hunk differs'
            original = original[:start] + base64.b64decode(change['before']) + original[end:]
        assert hashlib.sha256(original).hexdigest() == delta['before_sha256'], 'D03 Gizmo normalization did not recover the exact prior parent'
        for old, new in changes:
            assert original.count(new) == 1, new
            original = original.replace(new, old)
    if path.name in helper_changes:
        old, new = helper_changes[path.name]
        assert original.count(new) == 1, name
        original = original.replace(new, old, 1)
    if path.name == 'dungeon_palette_runtime.inc':
        building_join = b'#include "dungeon_authored_sample.inc"\n#include "dungeon_building_sample.inc"\n'
        assert original.count(building_join) == 1, 'Dungeon Building sample include seam differs'
        original = original.replace(building_join, b'#include "dungeon_authored_sample.inc"\n', 1)
        # The D01 include edit normalized this source to LF. Recover the
        # exact previous CRLF file with its one original LF include seam.
        assert b'\r' not in original, 'Dungeon Building palette newline form differs'
        original = original.replace(b'\n', b'\r\n')
        original = original.replace(b'#include "dungeon_cells_graph.inc"\r\n', b'#include "dungeon_cells_graph.inc"\n', 1)
        # Admit only the shared authored-reader include at its original owner seam.
        joined = b'#include "dungeon_cells_graph.inc"\n#include "dungeon_authored_sample.inc"\r\n#include "dungeon_cells_port.inc"'
        previous = b'#include "dungeon_cells_graph.inc"\r\n#include "dungeon_cells_port.inc"'
        assert original.count(joined) == 1, "Dungeon authored reader include seam differs"
        original = original.replace(joined, previous, 1)
    assert hashlib.sha256(original).hexdigest() == row['sha256'], name
    texts[name] = data.decode('utf-8')
    inputs.append({'path': str(path), 'sha256': hashlib.sha256(data).hexdigest()})

vendor = args.framework / 'third_party/vendor/minhook/src'
for name in ('trampoline.c', 'trampoline.h', 'buffer.c', 'buffer.h',
             'hde/hde64.c', 'hde/hde64.h', 'hde/table64.h', 'hde/pstdint.h'):
    path = vendor / name
    data = path.read_bytes()
    assert data == (args.source / 'include/vendor/minhook/src' / name).read_bytes(), name
    inputs.append({'path': str(path), 'sha256': hashlib.sha256(data).hexdigest()})

def declaration(text, name):
    match = re.search(r'constexpr npc_quest_list_ui_codes::Code ' + name + r'\{.*?\};', text, re.S)
    assert match, name
    return match[0].replace('npc_quest_list_ui_codes::Code ' + name, 'Code ui_code')

def guard(text):
    start = text.index('        std::array<unsigned char,')
    end = text.index('        std::memcpy(bytes.data(),receipt.pristine', start)
    return text[start:end]

generated = []
cases = []
for name in ('gizmo', 'config', 'sound', 'light', 'palette', 'drag', 'release', 'create'):
    cell = name in ('drag', 'release', 'create')
    text = texts[f'src/game/dungeon_{"cells" if cell else name}_runtime.inc']
    block = guard(text)
    setup = ''
    if cell:
        header = 'dungeon_cell_graph_codes.h' if name == 'create' else 'dungeon_cells_ui_codes.h'
        code = declaration(texts['src/game/' + header], name)
        match = re.search(r'route\(memory,dungeon_cells_codes::' + name + r',[^\n]*?,(\d+),error\)', text)
        assert match, name
        cut = int(match[1])
        setup = f'const auto& pin=ui_code;constexpr size_t copied={cut};\n'
    else:
        code = declaration(text, 'ui_code')
        cut = int(re.search(r'ui_code.prefix.begin\(\)\+(\d+)', block)[1])
    generated.append(f'''namespace {name} {{
{code}
bool check(Memory& memory,uintptr_t image,uintptr_t original,std::string& error){{
    const struct {{uintptr_t image;}} state{{image}};
    {setup}{block}
    return true;
}}
}}
''')
    cases.append(f'{{"{name}",&{name}::ui_code,{cut},&{name}::check}}')
    if name == 'gizmo':
        legacy = block.encode()
        for old, new in changes:
            legacy = legacy.replace(new, old)
        generated.append('namespace legacy_gizmo {\n' + code + '\n'
            'bool check(Memory& memory,uintptr_t image,uintptr_t original,std::string& error){\n'
            'const struct {uintptr_t image;} state{image};\n' + legacy.decode() +
            '\nreturn true;\n}\n}\n')
generated.append('inline const std::array<Case,8> cases{{\n' + ',\n'.join(cases) + '\n}};\n')
(args.output / 'dungeon_boundaries.inc').write_text('\n'.join(generated), encoding='utf-8')
(args.output / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
print('PASS source boundaries: six Gizmo boundary literals and exact shared-widget helper authentication changes admitted; all ownership, physical trampoline, full SHA and helper guards retained; seven related Dungeon cuts unchanged; Framework and combined MinHook sources identical')
