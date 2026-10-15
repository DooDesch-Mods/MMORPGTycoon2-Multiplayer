from pathlib import Path
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--source-root',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
source=args.source_root/'src/game/tutorial_replication/startingpoint_commit_intent.inc'
text=source.read_text()
start=text.index('bool read_startingpoint_row(')
args.output.mkdir(parents=True,exist_ok=True)
(args.output/'commit_intent_body.inc').write_text(text[start:])
