import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
text = (args.source / 'src/game/native_save_request.inc').read_text(encoding='utf-8')
start = text.rindex('namespace {', 0, text.index('uint32_t periodic_countdown_show'))
end = text.index('bool original_export_request(', start)
body = text[start:end]
if body.count('periodic_countdown_show(show,window)') != 1:
    raise SystemExit('Original countdown Show invocation differs')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(body, encoding='utf-8')
