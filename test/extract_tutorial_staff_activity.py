from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
t=(a.source_root/'src/game/tutorial_startingpoint_fixture/activity_read.inc').read_text();a.output.mkdir(parents=True,exist_ok=True)
for name,start,end in [('loc','bool activity_loc(', 'struct ActivityDeveloper'),('managers','struct ActivityManagers','struct ActivityFormatCalls'),('receipt','bool staff_activity_state(',None)]:
 first=t.index(start);last=t.index(end,first) if end else len(t)
 (a.output/(name+'.inc')).write_text(t[first:last])
