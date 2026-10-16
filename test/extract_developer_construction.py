from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
t=a.source.read_text(encoding='utf8');a.output.mkdir(parents=True,exist_ok=True)
def body(anchor):
 start=t.index(anchor);pos=t.index('{',start);depth=1;end=pos+1
 while depth:
  depth+=(t[end]=='{')-(t[end]=='}');end+=1
 return t[start:end]
for name,anchor,tail in [('operation','struct Operation {',';'),('build_operation','struct BuildOperation {',';'),('history_operation','struct HistoryOperation {',';'),('perform_place','action::Outcome perform_place(',''),('engine_jobs','bool construction_jobs(const action::Binding&',''),('runtime_jobs','bool construction_jobs(const action::Context&',''),('engine_bug_parent','bool bug_parent_alive(const action::Binding&',''),('runtime_bug_parent','bool bug_parent_alive(const action::Context&','')]:
 (a.output/f'construction_{name}.inc').write_text(body(anchor)+tail+'\n',encoding='utf8')
