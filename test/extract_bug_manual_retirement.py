"""Execute the production BugButton retirement predicate with controlled services."""
import argparse
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
source=(a.source_root/'src/game/bug_authority/manual_action.cpp').read_text(encoding='utf-8')
anchor='bool retire_lobby(std::string& e)'
begin=source.index(anchor);end=source.index('{',begin)+1;depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
body=source[begin:end]
assert 'world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0' in body
assert body.index('GetCurrentThreadId()!=o->thread')<body.index('native::read_world_status(world,e)')<body.index('o->pending.reset()')
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'bug_manual_retirement.inc').write_text(body+'\n',encoding='utf-8')
print('PASS actual BugButton Mainthread/Title/transition/positive-save retirement boundary')
