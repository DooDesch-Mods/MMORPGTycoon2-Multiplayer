from pathlib import Path
import argparse
import hashlib
import json

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--construction-source', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
construction = args.construction_source or args.root/'src/game/developer_staff/construction_native.cpp'
classifier = args.root/'src/game/remove_diagnostics.cpp'

def function(path, anchor):
    text = path.read_text(encoding='utf-8')
    start = text.index(anchor)
    end = text.index('{', start)+1
    depth = 1
    while depth:
        depth += (text[end] == '{')-(text[end] == '}')
        end += 1
    return text[start:end]

for name, source, anchor in [
    ('building_type_classify', classifier, 'bool classify_normal_building('),
    ('building_type_binding', construction, 'bool building(const Capture&'),
    ('building_type_graph', construction, 'bool graph(const Capture&'),
    ('building_type_jobs', construction, 'bool read_jobs(')]:
    (args.output/(name+'.inc')).write_text(function(source,anchor)+'\n',encoding='utf-8')
(args.output/'source_inputs.json').write_text(json.dumps({str(path):hashlib.sha256(path.read_bytes()).hexdigest()
    for path in (construction,classifier)},indent=2)+'\n')
