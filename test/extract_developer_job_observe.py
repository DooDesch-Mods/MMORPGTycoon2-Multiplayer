"""Extract the actual existing Developer observation body for controlled reads."""
from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=a.source.read_text(encoding='utf-8');start=s.index('bool observe_fixture(');end=s.index('\nbool run_fixture(',start)
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'developer_job_observe.inc').write_text(s[start:end],encoding='utf-8')
