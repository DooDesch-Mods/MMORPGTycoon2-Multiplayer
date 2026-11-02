import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
text = (args.source / 'src/game/typed_action/ordered_save_request.inc').read_text(encoding='utf-8')
start = text.index('    bool periodic_service(')
end = text.index('    bool quit_pending_=', start)
body = text[start:end]
if body.count('periodic_trigger_(error)') != 1 or body.count('periodic_requests_.completed()') != 1:
    raise SystemExit('Actual periodic service trigger or completion boundary differs')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(body, encoding='utf-8')
