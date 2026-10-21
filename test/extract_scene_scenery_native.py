from pathlib import Path
import argparse
import hashlib
import json

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()

def body(text, signature):
    start = text.index(signature)
    cursor = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[cursor] == '{') - (text[cursor] == '}')
        cursor += 1
    return text[start:cursor]

pieces = []
pins = []
for filename, signatures in (
    ('dungeon_input_sample.inc', ('dungeon_gizmo_control::Scenery scenery_value(', 'bool scenery_observed(', 'bool scenery_record(')),
    ('dungeon_gizmo_control_capture.inc', ('bool control_row(', 'bool publish_controls(')),
):
    text = (args.source / 'src/game' / filename).read_text(encoding='utf-8')
    for signature in signatures:
        actual = body(text, signature)
        pieces.append(actual)
        pins.append({'source': filename, 'signature': signature, 'sha256': hashlib.sha256(actual.encode()).hexdigest()})
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'scene_scenery_native_body.inc').write_text('\n\n'.join(pieces) + '\n', encoding='utf-8')
(args.output / 'scene_scenery_native_body.json').write_text(json.dumps(pins, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'status': 'EXTRACTED', 'actual_functions': len(pieces), 'cpp_or_native_executed': False}))
