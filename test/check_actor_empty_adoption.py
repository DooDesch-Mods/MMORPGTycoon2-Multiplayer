from pathlib import Path
import argparse

p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);a=p.parse_args()
s=(a.root/'src/game/intervention_actor_live.cpp').read_text(encoding='utf-8')
s=s[s.index('bool apply(const a::Context&'):s.index('bool monster_update(')]
stages=[
    'State before;Locals locals;if(!read_current(context,binding,before,locals,error)||!install_cache(std::move(locals),error))return false;',
    'for(const auto& row:work)',
    'if(!empty_adoption.begin(before,*frame.management->intervention_actors,state.locals,work,error))return failed();',
    'if(!empty_adoption.active()){',
    'Receipt adoption;const auto adopted=adopt_current(context,binding,*frame.management->intervention_actors,adoption,error);receipt.attempted|=adoption.attempted;receipt.attempted_stores+=adoption.attempted_stores;receipt.stores+=adoption.stores;if(!adopted)return failed();',
    'State after;if(!read_current(context,binding,after,locals,error))return failed();if(after!=*frame.management->intervention_actors)',
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
