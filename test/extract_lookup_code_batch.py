import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source-root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
base = args.source_root / 'src/game/object_identity/remove_runtime/copy'
reader = (base / 'world_prop_reader.cpp').read_text(encoding='utf-8')
batch = (base / 'saved_scenery_batch_runtime.inc').read_text(encoding='utf-8')

def body(text, anchor):
    start = text.index(anchor)
    opening = text.index('{', start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == '{': depth += 1
        elif text[pos] == '}':
            depth -= 1
            if depth == 0: return text[start:pos + 1]
    raise ValueError('Unclosed actual body: ' + anchor)

parts = [body(reader, 'static bool resolve_lookup_raw('), body(reader, 'bool resolve_lookup(')]
methods = [body(batch, name) for name in ('bool lookup_context(', 'bool resolve_cached_lookup(', 'bool verify_lookup(', 'bool finish(std::string& error)')]
out = args.output
out.mkdir(parents=True, exist_ok=True)
(out / 'lookup_raw.inc').write_text(parts[0] + '\n', encoding='utf-8')
(out / 'lookup_batch_methods.inc').write_text('\n'.join(methods) + '\n', encoding='utf-8')
(out / 'lookup_batch_dispatch.inc').write_text(body(batch, 'bool RootSceneryReadBatch::resolve_current_lookup(') + '\n' + parts[1] + '\n', encoding='utf-8')
print('Extracted actual lookup verifier and batch methods')
