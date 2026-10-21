from pathlib import Path
import argparse,hashlib,json,re
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
path=a.source/'src/game/character_import_runtime.inc';text=path.read_text(encoding='utf-8');receipts=[]
for signature,name in [('bool import_presentation(','character_import_tail_body.inc')]:
    assert text.count(signature)==1;begin=text.index(signature);end=text.index('{',begin)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    body=text[begin:end]+'\n';(a.output/name).write_text(body,encoding='utf-8');receipts.append({'signature':signature,'sha256':hashlib.sha256(body.encode()).hexdigest()})
(a.output/'receipt.json').write_text(json.dumps({'input_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bodies':receipts},indent=2)+'\n',encoding='utf-8')
assert 'constexpr size_t copied=6;std::array<unsigned char,copied+14> trampoline' in text
for guard in ('receipt.pristine+copied','trampoline.begin()+copied','resume!=state.image+0x689510+copied'):
    assert text.count(guard)==1,guard
prefix=bytes.fromhex('4157415641554154555756534881ec180200004889d34889ce488d155354e800')
assert len(prefix)==32
assert re.search(r'\{0x689510,1952,"0d58c3c21b8a9760d26d8ecd7bef73f385c617326f425e384a8c245a69c7ff5c"\}',(a.source/'src/game/character_import_codes.h').read_text())
case='{"character_import_image",{'+','.join(map(str,prefix))+'},6,6,6,20,12}'
(a.output/'native_ui_boundaries.inc').write_text('inline constexpr std::array<BoundaryCase,1> cases{{\n'+case+'\n}};\n',encoding='utf-8')
print('Extracted complete actual Character import scoped presentation body')
