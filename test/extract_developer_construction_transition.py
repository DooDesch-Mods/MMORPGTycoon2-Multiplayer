from pathlib import Path
import argparse

p=argparse.ArgumentParser()
p.add_argument('--source',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
source=a.source.read_text(encoding='utf8')
a.output.mkdir(parents=True,exist_ok=True)

def body(anchor,tail=''):
    begin=source.index(anchor)
    start=source.index('{',begin)
    depth=1;end=start+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin:end]+tail+'\n'

for name,anchor,tail in (
    ('actor','struct Actor {',';'),
    ('quiet','bool quiet(',''),
    ('work','struct Work {',';'),
    ('call','bool call(Work&',''),
    ('member_ids','std::vector<int32_t> member_ids(',''),
    ('actor_slot','size_t actor_slot(',''),
    ('target_plan','bool target_plan(',''),
    ('apply','bool apply(const action_session::Context&','')):
    (a.output/f'construction_transition_{name}.inc').write_text(body(anchor,tail),encoding='utf8')
