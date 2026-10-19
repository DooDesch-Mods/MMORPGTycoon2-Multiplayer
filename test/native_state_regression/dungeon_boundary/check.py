from pathlib import Path
import argparse
import hashlib
import json
import re

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--framework', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--overlay', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parent

changes = (
    (b'array<unsigned char,20> trampoline', b'array<unsigned char,19> trampoline'),
    (b'trampoline.data()+12,8', b'trampoline.data()+11,8'),
    (b'ui_code.prefix.begin()+6', b'ui_code.prefix.begin()+5'),
    (b'trampoline.begin()+6', b'trampoline.begin()+5'),
    (b'resume!=state.image+ui_code.rva+6', b'resume!=state.image+ui_code.rva+5'),
    (b'original six-byte instruction', b'original five-byte instruction'),
)
texts = {}
inputs = []
for row in json.loads((root / 'baseline.json').read_text()):
    name = row['path']
    path = args.source / name
    if args.overlay and (args.overlay / name).exists():
        path = args.overlay / name
    data = path.read_bytes()
    original = data
    if name.endswith('dungeon_gizmo_runtime.inc'):
        for old, new in changes:
            assert original.count(new) == 1, new
            original = original.replace(new, old)
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
print('PASS source boundaries: only six Gizmo boundary literals change; all ownership, physical trampoline, full SHA and helper guards retained; seven related Dungeon cuts unchanged; Framework and combined MinHook sources identical')
