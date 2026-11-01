from pathlib import Path
import argparse,hashlib

p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path);a=p.parse_args()
s=(a.root/'src/game/intervention_actor_live.cpp').read_text(encoding='utf-8')
s=s[s.index('bool apply(const a::Context&'):s.index('bool monster_update(')]
stages=[
    'State before;Locals locals;if(!read_current(context,binding,before,locals,error,empty_candidate?&first_empty:nullptr)||!install_cache(std::move(locals),error))return false;',
    'for(const auto& row:work)',
    'if(!empty_adoption.begin(before,*frame.management->intervention_actors,state.locals,work,error))return failed();',
    'if(!empty_adoption.active()){',
    'Receipt adoption;const auto adopted=adopt_current(context,binding,*frame.management->intervention_actors,adoption,error);receipt.attempted|=adoption.attempted;receipt.attempted_stores+=adoption.attempted_stores;receipt.stores+=adoption.stores;if(!adopted)return failed();',
    'State after;if(!read_current(context,binding,after,locals,error,first_empty?&last_empty:nullptr))return failed();',
    'if(first_empty&&!same_empty_read(first_empty,last_empty,error))return failed();',
    'if(after!=*frame.management->intervention_actors)',
    'if(!empty_adoption.finish(error))return failed();',
    'if(!state.client.confirm(*state.source,journal,work,error))return failed();',
    'receipt.confirmed=true;error.clear();return true;']
positions=[]
for stage in stages:
    assert s.count(stage)==1,stage
    positions.append(s.index(stage))
assert positions==sorted(positions),'Empty Actor guard escaped fresh census, native fallback or lifecycle confirmation boundaries'
assert s.count('install_cache(std::move(locals),error)')==2
assert s.count('adopt_current(')==1
assert 'context.role!=a::Role::Client||!exact(context,binding,error)||!frame_bound(frame,error)||!routes(error)||state.ordered' in s
print('Actor empty adoption retains complete first/final census, original fallback and journal confirmation')

assert 'empty_candidate=work.empty()&&frame.management->intervention_actors->empty()&&state.locals.empty()' in s
baseline=s.replace('    EmptyRead first_empty,last_empty;const bool empty_candidate=work.empty()&&frame.management->intervention_actors->empty()&&state.locals.empty();\n','',1)
baseline=baseline.replace(',error,empty_candidate?&first_empty:nullptr)',',error)',1).replace(',error,first_empty?&last_empty:nullptr)',',error)',1)
baseline=baseline.replace('\n    if(first_empty&&!same_empty_read(first_empty,last_empty,error))return failed();','',1)
assert hashlib.sha256(baseline.encode()).hexdigest()=='57daff5988ee0c119e705011bbebdaa86b06aa76e8049d0d94e3a0f56178819d','Actor lifecycle/nonempty apply changed beyond empty pair wiring'
native=(a.root/'src/game/intervention_actor_runtime.cpp').read_text(encoding='utf-8')
baseline_runtime=native.replace(native[native.index('struct EmptyReadWitness {'):native.index('bool read_bound(')],'',1)
baseline_runtime=baseline_runtime.replace('std::string& error,EmptyRead* empty_pair){','std::string& error){',1).replace('    if(empty_pair)empty_pair->reset();\n','',1)
begin=baseline_runtime.index('    if(empty_pair&&before.actors.empty()){')
end=baseline_runtime.index('    Census after;',begin)
baseline_runtime=baseline_runtime[:begin]+baseline_runtime[end:]
assert hashlib.sha256(baseline_runtime.encode()).hexdigest()=='f28fd9ee0ee81fe85ed4d17f957c7f24b88ab47513fd3656afce71eb8146d705','Actor nonempty/standalone reader changed beyond empty pair deferral'
if a.output:
    a.output.mkdir(parents=True,exist_ok=True)
    body=native[native.index('bool same(const Census&'):native.index('bool read_bound(')]
    body=body.replace('\n}\nstruct EmptyReadWitness','\nstruct EmptyReadWitness',1)
    body+=native[native.index('bool read_current('):native.rfind('\n}')]
    (a.output/'actor_empty_read_actual.inc').write_text(body,encoding='utf-8')
