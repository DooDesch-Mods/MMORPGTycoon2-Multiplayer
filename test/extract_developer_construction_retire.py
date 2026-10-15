from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=a.source.read_text();begin=s.index('bool retire(Work&');start=s.index('{',begin);depth=1;end=start+1
while depth:
    if s[end]=='{':depth+=1
    elif s[end]=='}':depth-=1
    end+=1
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(s[begin:end]+'\n')
