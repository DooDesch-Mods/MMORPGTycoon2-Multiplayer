"""Extract only the actual staged separate focus API bodies for mock execution."""
import hashlib
import argparse
from pathlib import Path
HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument("--source",type=Path,default=HERE/"movement_process.cpp")
parser.add_argument("--output",type=Path,default=HERE/"focus_bodies.inc")
args=parser.parse_args()
source=args.source.read_text(encoding="utf-8")
def body(signature):
    start=source.index(signature)
    opening=source.index('{',start)
    depth=1;end=opening+1
    while depth:
        if source[end]=='{':depth+=1
        if source[end]=='}':depth-=1
        end+=1
    return source[start:end]+"\n"
result=body('bool read_focus(FocusReceipt&')+body('bool request_focus(const std::string&')+body('bool idle_world(const native::WorldStatus&')+body('bool keyboard_admission(uint32_t')
cursor_guard=next(line.strip() for line in source.splitlines() if line.strip().startswith('if(step.scenario!='))
result+='bool wasd_admission(const tutorial_replication::Cursor& step,std::string& e){'+cursor_guard+'return true;}\n'
args.output.write_text(result,encoding="utf-8")
print("focus actual-body source SHA256="+hashlib.sha256(args.source.read_bytes()).hexdigest())
