"""Extract the three actual Ability production trampoline guards."""
from pathlib import Path
import argparse,hashlib,json,re
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
runtime=args.source/'src/game/ability_membership_runtime.inc';text=runtime.read_text()
for guard in ['trampoline.data()+copied[index]+6','receipt.pristine+copied[index]','trampoline.begin()+copied[index]','resume!=state.image+pin.rva+copied[index]']:
    if text.count(guard)!=1:raise RuntimeError('Ability actual production continuation guard changed: '+guard)
match=re.search(r'constexpr std::array<size_t,3> copied\{([^}]+)\}',text)
if not match:raise RuntimeError('Actual Ability copied-length array is absent')
copied=[int(value.strip()) for value in match[1].split(',')]
pins=args.source/'src/game/ability_membership_codes.h';codes=pins.read_text()
original=args.source/'test/ability_membership_boundaries.json'
cases=json.loads(original.read_text())
if len(cases)!=3:raise RuntimeError('Ability original route metadata extent differs')
rows=[]
for i,case in enumerate(cases):
    name=case['name'];rva=int(case['rva'],16);size=case['bytes'];sha=case['sha256'];prefix=case['prefix32']
    pin=re.search(r'\{'+hex(rva)+r',(\d+),"([0-9a-f]{64})"\}',codes)
    if not pin:raise RuntimeError('Complete Ability production body pin is absent: '+hex(rva))
    if size and int(pin[1])!=size:raise RuntimeError('Ability original full body extent changed: '+hex(rva))
    if sha and pin[2]!=sha:raise RuntimeError('Ability original full body digest changed: '+hex(rva))
    raw=bytes.fromhex(prefix)
    if len(raw)!=32:raise RuntimeError('Ability original first32 extent differs')
    count=copied[i];rows.append('{"'+name+'",{'+','.join(map(str,raw))+'},'+','.join(map(str,[count,count,count,count+14,count+6]))+'}')
(args.output/'native_ui_boundaries.inc').write_text('inline constexpr std::array<BoundaryCase,3> cases{{\n'+',\n'.join(rows)+'\n}};\n')
(args.output/'cases.json').write_text(json.dumps({'runtime_sha256':hashlib.sha256(runtime.read_bytes()).hexdigest(),
    'codes_sha256':hashlib.sha256(pins.read_bytes()).hexdigest(),'original_metadata_sha256':hashlib.sha256(original.read_bytes()).hexdigest(),
    'copied':copied,'native_calls':0},indent=2)+'\n')
print('Extracted actual three Ability production boundaries; own disabled MinHook only')
