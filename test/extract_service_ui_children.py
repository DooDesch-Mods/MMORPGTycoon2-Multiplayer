import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source-root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.source_root / 'src/game/management_service_ui.inc').read_text(encoding='utf-8')
start = source.index('bool service_ui_priceable(')
end = source.index('\nbool service_ui_find(', start)
body = source[start:end]
assert body.count('loan_ui_array(') == 1
assert source.count('service_ui_children(r,pane,klass,out.window,pending,e)') == 2
assert '0x8167c0,592' in source and '0xc35730,256' in source
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'service_ui_children.inc').write_text(body + '\n', encoding='utf-8')
print('Extracted actual Service UI child traversal')
