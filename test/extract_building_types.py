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

# Both original lookup guards remain exact; only their failure metadata changes.
catalog = args.root/'src/game/scenery_kiss/native_catalog.cpp'
lookup_helper = function(catalog, 'bool require_registry(')
for anchor, stage, digest in [('bool bind(uint64_t uid,Binding& b,std::string& e)override', 'ActualRoot.bind', 'e477dc075c7dc9e055f3d68f06cd1dd753bb0c0bd7584b050754a6ca3ddd9263'), ('bool read_action_prop(', 'read_action_prop', '127e71b3581dbe64886aef443626cc212ffd1ff5b95b42f7e2edf152d7ba7994')]:
    actual = function(catalog, anchor)
    if stage == 'ActualRoot.bind':
        added = '        {\n            mt2mp::game::live_host_state::apply_timing::PageReadScope owner_fields;\n            if(!memory.copy(object+0x170,&shard,8,e)||!memory.copy(object+0x178,&region,8,e))return false;\n        }'
        assert actual.count(added) == 1
        actual = actual.replace(added, '        if(!memory.copy(object+0x170,&shard,8,e)||!memory.copy(object+0x178,&region,8,e))return false;', 1)
    else:
        timer = '    mt2mp::game::character_catalogue_timing::Scope measured(mt2mp::game::character_catalogue_timing::Part::PropRead);\n'
        added = '    host::ProcessMemory memory;host::LookupApi api;\n    {mt2mp::game::live_host_state::apply_timing::PageReadScope lookup_fields;\n        if(!host::resolve_lookup(memory,image,hash,api,e))return false;}'
        assert actual.count(timer) == 1 and actual.count(added) == 1
        actual = actual.replace(timer, '', 1).replace(added, '    host::ProcessMemory memory;host::LookupApi api;if(!host::resolve_lookup(memory,image,hash,api,e))return false;', 1)
    edge = 'if(!require_registry(uid,object,seh,"'+stage+'",e))return false;'
    assert actual.count(edge)==1, 'Missing exact original lookup diagnostic edge: '+stage
    baseline = actual.replace(edge, 'if(seh||!object){e="scenery_action_prop_registry_failed SEH="+std::to_string(seh);return false;}', 1)
    assert hashlib.sha256(baseline.encode()).hexdigest()==digest, 'Scenery lookup changed outside UID/stage diagnostic: '+stage
(args.output/'scenery_registry_guard.inc').write_text(lookup_helper+'\n',encoding='utf-8')

(args.output/'construction_duplicate_owners.inc').write_text(function(construction,'bool owners(')+'\n',encoding='utf-8')
(args.output/'construction_duplicate_origin.inc').write_text(function(args.root/'src/game/developer_staff/read.cpp','bool same_origin(')+'\n',encoding='utf-8')
