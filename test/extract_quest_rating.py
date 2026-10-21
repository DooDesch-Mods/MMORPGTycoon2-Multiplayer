from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
rows=[]
def extract(source,begin,end,name,change=None):
 text=(a.source_root/source).read_text();i=text.index(begin);j=text.index(end,i+len(begin));body=text[i:j]
 if change:body=change(body)
 (a.output/name).write_text(body)
 rows.append({'source':source,'source_sha256':hashlib.sha256((a.source_root/source).read_bytes()).hexdigest(),'output':name,'body_sha256':hashlib.sha256(body.encode()).hexdigest()})
extract('src/game/typed_action_runtime.cpp','bool network_binding(','bool observe_startingpoint_submission(','quest_rating_idle.inc')
extract('src/game/live_host_state/economic_fence.h','bool quest_binding(','bool allow(EconomicLeaf','quest_rating_binding.inc')
extract('src/game/live_host_state/day_rollover/economy/native_quest_runtime.inc','template<class Guard>bool store_quest_runtime','\n}', 'quest_rating_scalar.inc')
# The function end marker above selects its outer closing brace; append it.
f=a.output/'quest_rating_scalar.inc';f.write_text(f.read_text()+'\n}\n')
extract('src/game/npc_quest_rating_runtime.inc','struct Calls {','json value(','quest_rating_calls.inc')
extract('src/game/npc_quest_authoring_runtime.inc','json status(){','#include "npc_quest_authoring_ui.inc"','quest_status_body.inc')
extract('src/game/live_host_state/bridge.cpp','bool Bridge::tick(','\nbool Bridge::before_result(', 'quest_rating_bridge_tick.inc')
extract('src/game/live_host_state/bridge.cpp','bool same_tutorial_publication(','\n}', 'quest_rating_tutorial_compare.inc')
f=a.output/'quest_rating_tutorial_compare.inc';f.write_text(f.read_text()+'\n}\n')
for row in rows:row['body_sha256']=hashlib.sha256((a.output/row['output']).read_bytes()).hexdigest()
(a.output/'extraction.json').write_text(json.dumps(rows,indent=2)+'\n')
