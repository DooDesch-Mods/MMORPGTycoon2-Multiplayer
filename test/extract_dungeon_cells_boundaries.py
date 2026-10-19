"""Extract all three actual Dungeon cell route prefixes and production guard spans."""
from pathlib import Path
import argparse,hashlib,json,re
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
source=args.source/'src/game/dungeon_cells_runtime.inc';text=source.read_text()
pins=args.source/'src/game/dungeon_cells_ui_codes.h';definitions=pins.read_text()
for required in ['trampoline.data()+copied+6','pin.prefix.begin()+copied','trampoline.begin()+copied','resume!=state.image+pin.rva+copied','memory.copy(original,trampoline.data(),copied+14']:
    if text.count(required)!=1:raise RuntimeError('Dungeon cell production boundary guard changed: '+required)
rows=[];cases=[]
for name in ['drag','release','create']:
    if name=='create':pins=args.source/'src/game/dungeon_cell_graph_codes.h';definitions=pins.read_text()
    pin=re.search(r'Code '+name+r'\{(0x[0-9a-f]+),(\d+),"([0-9a-f]{64})",\{([^}]+)\}\};',definitions)
    call=re.findall(r'route\(memory,dungeon_cells_codes::'+name+r',uintptr_t\(&'+name+r'_entry\),uintptr_t\(original_'+name+r'\),'+name+r'_generation,(\d+),error\)',text)
    if not pin or len(call)!=1:raise RuntimeError('Dungeon cell actual route pin/call changed: '+name)
    copied=int(call[0]);prefix=[int(n,0) for n in pin[4].split(',')]
    if len(prefix)!=32 or copied>11:raise RuntimeError('Dungeon cell actual route prefix/span changed: '+name)
    cases.append('{"dungeon_cells_'+name+'",{'+','.join(map(str,prefix))+'},'+','.join(map(str,[copied,copied,copied,copied+14,copied+6]))+'}')
    rows.append({'route':name,'rva':pin[1],'body_sha256':pin[3],'copied':copied,'runtime_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'pins_sha256':hashlib.sha256(pins.read_bytes()).hexdigest()})
(args.output/'native_ui_boundaries.inc').write_text('inline constexpr std::array<BoundaryCase,3> cases{{\n'+',\n'.join(cases)+'\n}};\n')
(args.output/'cases.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Extracted three actual Source210 Dungeon cell route and production guard cases')
