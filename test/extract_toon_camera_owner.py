import argparse
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--source-root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
source=(a.source_root/'src/game/toon_time_fence/runtime.inc').read_text(encoding='utf-8')
a.output.mkdir(parents=True,exist_ok=True)
for marker,name in [('bool toon_camera_owner(','toon_camera_owner.inc'),('bool toon_motion_sample(','toon_motion_sample.inc')]:
    start=source.index(marker);opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    (a.output/name).write_text(source[start:end],encoding='utf-8')
print('Extracted actual Toon camera owner and trace sample bodies')
