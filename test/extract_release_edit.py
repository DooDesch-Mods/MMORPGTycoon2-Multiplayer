from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=a.source_root/'src/game/release_runtime.cpp';text=source.read_text(encoding='utf-8')
start='bool defaults(Runtime& r,';end='bool collection(Runtime& r,'
assert text.count(start)==1 and text.count(end)==1
body=text[text.index(start):text.index(end)]
a.output.mkdir(parents=True,exist_ok=True);target=a.output/'release_edit_actual.inc';target.write_text(body,encoding='utf-8')
(a.output/'release_edit_actual.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'body_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'functions':['defaults']},indent=2)+'\n',encoding='utf-8')

# Use current production retirement bodies. WorldStatus is the real header type
# in the fixture so the Title-only save-counter sentinel stays representative.
for filename, stem in [('release_runtime.cpp', 'release'), ('general_project_runtime.cpp', 'general_project')]:
    actual = (a.source_root / 'src/game' / filename).read_text(encoding='utf-8')
    anchor = 'bool retire_lobby(std::string& e)'
    assert actual.count(anchor) == 1
    begin = actual.index(anchor)
    end = actual.index('{', begin) + 1
    depth = 1
    while depth:
        depth += (actual[end] == '{') - (actual[end] == '}')
        end += 1
    (a.output / (stem + '_retire_actual.inc')).write_text(actual[begin:end] + '\n', encoding='utf-8')
