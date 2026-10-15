"""Extract the actual hook confirmation body without changing its call path."""
from pathlib import Path
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--source-root',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
source=(args.source_root/'src/game/tutorial_replication/native.cpp').read_text()
start=source.index('bool hooks_enabled(std::string& e){')
end=source.index('bool verify_pay_entry(',start)
body=source[start:end]
if body.count('bool hooks_enabled(')!=1 or not body.rstrip().endswith('}'):
    raise SystemExit('tutorial hook confirmation extraction boundary differs')
args.output.mkdir(parents=True,exist_ok=True)
(args.output/'tutorial_hook_confirmation.inc').write_text(body)
