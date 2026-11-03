from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
receipt={}
def extract(file,anchor,name):
    text=(a.source/file).read_text();start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    body=text[start:end]+'\n';(a.output/name).write_text(body,newline='\n');receipt[name]={'source':file,'sha256':hashlib.sha256(body.encode()).hexdigest()}
for anchor,name in [('bool origin(','tutorial_origin_actual.inc'),('bool native_state(','tutorial_state_actual.inc'),('bool capture_intro(','tutorial_capture_intro_actual.inc'),('bool capture(const action_session::Context&','tutorial_capture_actual.inc')]:
    extract('src/game/tutorial_replication/native.cpp',anchor,name)
extract('src/game/tutorial_replication/intro_native.inc','bool sample(','tutorial_intro_sample_actual.inc')
(a.output/'body-hashes.json').write_text(json.dumps(receipt,indent=2)+'\n')
