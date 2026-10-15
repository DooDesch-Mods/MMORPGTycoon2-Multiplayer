from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=(a.source_root/'src/game/live_host_state/day_rollover/economy/native_quest_state.inc').read_text();start='bool NativePort::store_npc_state(';assert s.count(start)==1
a.output.mkdir(parents=True,exist_ok=True);(a.output/'npc_state_guard.inc').write_text(s[s.index(start):])
