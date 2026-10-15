from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
folder=a.source_root/'src/game/tutorial_replication'
owner=(folder/'lifecycle_owner.inc').read_text();hook=(folder/'lifecycle_hook.inc').read_text();native=(folder/'native.cpp').read_text()
def part(s,start,end):
    assert s.count(start)==1 and s.count(end)==1
    return s[s.index(start):s.index(end)]
a.output.mkdir(parents=True,exist_ok=True)
bodies={'load_start_owners.inc':part(owner,'bool array(','bool owned_cursor('),
        'load_start_hook.inc':part(hook,'void scenario_start(', '    World w;action_session::Context c;action_session::Binding b;std::string e,id;bool observe=false;')+'}\n',
        'load_start_finish.inc':part(native,'bool finish_client_load(', 'void invalidate(')}
for name,body in bodies.items():(a.output/name).write_text(body)
