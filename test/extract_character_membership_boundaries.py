"""Extract the exact confirmed Character delete production boundary."""
from pathlib import Path
import argparse,hashlib,json,re
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
runtime=args.source/'src/game/character_membership_runtime.inc';text=runtime.read_text()
required=['trampoline.data()+copied+6','receipt.pristine+copied','trampoline.begin()+copied','resume!=state.image+pin.rva+copied']
for guard in required:
    if text.count(guard)!=1:raise RuntimeError('Character confirmation actual production guard changed: '+guard)
match=re.search(r'constexpr size_t copied=(\d+);std::array<unsigned char,copied\+14> trampoline',text)
if not match:raise RuntimeError('Character confirmation actual original boundary constant absent')
copied=int(match[1]);prefix=bytes.fromhex('555756534883ec68488b9160040000488b02488b80a80000004889cb488d0dad')
if len(prefix)!=32:raise RuntimeError('Exact Character native prefix size differs')
pins=args.source/'src/game/character_membership_codes.h'
body_sha='6f2b40b04688ab95a67ffec6a0b71513943fffd8980bed8f3f55ced5b6c2f7b1'
if not re.search(r'\{0x684250,276,"'+body_sha+r'"\}',pins.read_text()):
    raise RuntimeError('Character confirmation complete original production body pin changed')
case='{"character_membership_confirm",{'+','.join(map(str,prefix))+'},'+','.join(map(str,[copied,copied,copied,copied+14,copied+6]))+'}'
(args.output/'native_ui_boundaries.inc').write_text('inline constexpr std::array<BoundaryCase,1> cases{{\n'+case+'\n}};\n')
(args.output/'cases.json').write_text(json.dumps({'runtime_sha256':hashlib.sha256(runtime.read_bytes()).hexdigest(),'codes_sha256':hashlib.sha256(pins.read_bytes()).hexdigest(),'body_sha256':body_sha,'rva':'0x684250','copied':copied,'native_calls':0},indent=2)+'\n')
print('Extracted actual confirmed Character delete guard; owned disabled MinHook test only')
