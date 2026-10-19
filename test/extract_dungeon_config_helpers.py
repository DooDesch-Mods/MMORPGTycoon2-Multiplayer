"""Extract the actual Dungeon config helper guard loop for the route regression."""
from pathlib import Path
import argparse
import hashlib
import json

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('source',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
path=args.source/'src/game/dungeon_config_runtime.inc'
text=path.read_text(encoding='utf-8-sig')
begin='    for(const auto& pin:dungeon_config_codes::helpers)'
end='    return true;\n}'
if text.count(begin)!=1:raise RuntimeError('Dungeon config actual helper-loop anchor changed')
start=text.index(begin)
finish=text.index(end,start)+len(end)
actual='bool check_helpers(Memory& memory,std::string& error){\n    std::string bytes,hash;\n'+text[start:finish]+'\n'
args.output.mkdir(parents=True,exist_ok=True)
(args.output/'dungeon_config_helpers_actual.inc').write_text(actual,encoding='utf-8')
(args.output/'bodies.json').write_text(json.dumps({'dungeon_config_helpers_actual.inc':{
    'source':str(path),'sha256':hashlib.sha256(actual.encode()).hexdigest()}},indent=2)+'\n')
