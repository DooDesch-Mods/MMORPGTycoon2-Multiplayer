from pathlib import Path
import argparse,json,hashlib

def method(source,name):
    start=source.index('    bool '+name+'(')
    brace=source.index('{',start)
    depth=1;index=brace+1;quote=None;escaped=False
    while depth:
        char=source[index]
        if quote:
            if escaped:escaped=False
            elif char=='\\':escaped=True
            elif char==quote:quote=None
        elif char in ('"',"'"):quote=char
        elif char=='{':depth+=1
        elif char=='}':depth-=1
        index+=1
    return source[start:index]

parser=argparse.ArgumentParser()
parser.add_argument('--candidate',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parent
baseline=root/'baseline_party_frame_native.txt'
candidate=args.candidate
runtime=candidate.parent/'scene_clone/runtime.cpp'
sources=[path.read_text(encoding='utf-8') for path in (baseline,candidate,runtime)]
methods=['\n'.join(method(source,name) for name in ('prepare_capture','capture')) for source in sources[:2]]
guards=[line.strip() for line in sources[2].splitlines() if 'if(!state_||!state_->prepared' in line]
if len(guards)!=1:raise RuntimeError('Exact original Scene confirmation admission guard not found')
if sources[2].count('state_->prepared=false;')!=1:raise RuntimeError('Exact consume-on-confirm statement not found')
template=(root/'capture_fixture.in').read_text(encoding='utf-8')
output=template.replace('@CONFIRM_GUARD@',guards[0]).replace('@CONSUME_PREPARATION@','state_->prepared=false;').replace('@BASELINE_METHODS@',methods[0]).replace('@CANDIDATE_METHODS@',methods[1])
target=args.output
target.parent.mkdir(parents=True,exist_ok=True);target.write_text(output,encoding='utf-8')
print(json.dumps({'generated':str(target),'source_sha256':{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in (baseline,candidate,runtime)},'baseline_prepares':methods[0].count('scenes_.prepare('),'candidate_prepares':methods[1].count('scenes_.prepare(')}))
