"""Print an exact bounded source range."""
import argparse
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('path',type=Path)
parser.add_argument('start',type=int)
parser.add_argument('end',type=int)
args=parser.parse_args()
lines=args.path.read_text(encoding='utf-8').splitlines()
for index in range(args.start-1,min(args.end,len(lines))): print(f'{index+1}:{lines[index]}')
