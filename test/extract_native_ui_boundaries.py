"""Extract the six actual UI prefixes and trampoline guard constants."""
from pathlib import Path
import argparse
import hashlib
import json
import re

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('source',type=Path)
parser.add_argument('output',type=Path)
parser.add_argument('--legacy-palette',type=Path)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
families=('ability_edit','costume_edit','dungeon_config','dungeon_light','dungeon_sound','dungeon_palette')
rows=[];code=[]
for family in families:
    path=args.source/'src/game'/(family+'_runtime.inc');text=path.read_text(encoding='utf-8-sig')
    pin=re.search(r'constexpr npc_quest_list_ui_codes::Code ui_code\{(0x[0-9a-f]+),(\d+),"([0-9a-f]{64})",\s*\{([^}]+)\}\};',text)
    if not pin:raise RuntimeError('Native UI prefix pin changed: '+family)
    prefix=[int(part.strip(),0) for part in pin[4].split(',')]
    if len(prefix)!=32:raise RuntimeError('Native UI current prefix must have32 bytes: '+family)
    def number(pattern):
        found=re.findall(pattern,text)
        if len(found)!=1:raise RuntimeError('Native UI guard extraction anchor changed: '+family+' '+pattern)
        return int(found[0])
    size=number(r'std::array<unsigned char,(\d+)> trampoline')
    offset=number(r'&resume,trampoline\.data\(\)\+(\d+),8')
    copied=number(r'ui_code\.prefix\.begin\(\)\+(\d+)')
    jump=number(r'continuation\.end\(\),trampoline\.begin\(\)\+(\d+)')
    resume=number(r'resume!=state\.image\+ui_code\.rva\+(\d+)')
    rows.append({'family':family,'source':str(path),'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                 'rva':pin[1],'pinned_body_sha256':pin[3],'copied':copied,'jump':jump,'resume':resume,'size':size,'resume_offset':offset})
    code.append('{"'+family+'",{'+','.join(str(byte) for byte in prefix)+'},'+','.join(str(value) for value in (copied,jump,resume,size,offset))+'}')
header='inline constexpr std::array<BoundaryCase,6> cases{{\n'+',\n'.join(code)+'\n}};\n'
if args.legacy_palette:
    text=args.legacy_palette.read_text(encoding='utf-8-sig');family='dungeon_palette_legacy'
    pin=re.search(r'constexpr npc_quest_list_ui_codes::Code ui_code\{(0x[0-9a-f]+),(\d+),"([0-9a-f]{64})",\s*\{([^}]+)\}\};',text)
    if not pin:raise RuntimeError('Retained original Palette prefix pin changed')
    prefix=[int(part.strip(),0) for part in pin[4].split(',')]
    values=(number(r'ui_code\.prefix\.begin\(\)\+(\d+)'),number(r'continuation\.end\(\),trampoline\.begin\(\)\+(\d+)'),
            number(r'resume!=state\.image\+ui_code\.rva\+(\d+)'),number(r'std::array<unsigned char,(\d+)> trampoline'),number(r'&resume,trampoline\.data\(\)\+(\d+),8'))
    header+='#define MT2MP_NATIVE_UI_LEGACY_CASE 1\ninline constexpr BoundaryCase legacy_palette_case{"dungeon_palette_legacy",{'+','.join(str(byte) for byte in prefix)+'},'+','.join(str(value) for value in values)+'};\n'
    rows.append({'family':family,'source':str(args.legacy_palette),'source_sha256':hashlib.sha256(args.legacy_palette.read_bytes()).hexdigest(),'guard':values,'expected':'actual MinHook guard rejection'})
(args.output/'native_ui_boundaries.inc').write_text(header,encoding='utf-8')
(args.output/'cases.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Extracted six actual native UI prefix/guard cases')
