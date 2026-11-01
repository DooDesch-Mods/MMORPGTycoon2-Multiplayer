from pathlib import Path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
domain = (args.root / 'src/game/framework_lobby_domain.inc').read_text(encoding='utf-8')
anchor = 'void update_busy_picker('
assert domain.count(anchor) == 1
start = domain.index(anchor)
end = domain.index('\nbool render_context(', start)
(args.output / 'actual_body.inc').write_text(domain[start:end] + '\n', encoding='utf-8')
flow = (args.root / 'src/game/prepare_flow.cpp').read_text(encoding='utf-8')
assert flow.count('enum class Step {') == 1
assert flow.count('bool status_is_handshake(') == 1
start = flow.index('enum class Step {')
end = flow.index(';', start) + 1
predicate = flow.index('bool status_is_handshake(')
predicate_end = flow.index('\n', predicate)
(args.output / 'handshake_body.inc').write_text(flow[start:end] + '\nnamespace lobby_prepare { ' + flow[predicate:predicate_end] + ' }\n', encoding='utf-8')
