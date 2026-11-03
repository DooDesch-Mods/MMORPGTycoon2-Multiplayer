"""Extract the production local parent and epoch readers for the native fixture."""
from pathlib import Path
import argparse
import hashlib

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()

def body(text, anchor):
    start = text.index(anchor)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

source = (args.root / 'src/net/snapshot_sync.cpp').read_text(encoding='utf-8')
epochs = [body(source, 'static uint64_t software_epoch('), body(source, 'uint64_t current_world_epoch() {'),
          body(source, 'uint64_t current_world_epoch(game::')]
(args.output / 'ordered_local_epoch.inc').write_text('\n'.join(epochs) + '\n', encoding='utf-8')
source = (args.root / 'src/game/typed_action/sidecar_adapter.cpp').read_text(encoding='utf-8')
parent = body(source, 'static bool ordered_local_context(')
baseline = parent.replace('        game::building_mixed_identity::opened_file::ReadOriginTransaction transaction;\n', '')
baseline = baseline.replace('proof,cause,transaction)', 'proof,cause)').replace('current_world_epoch(transaction)', 'current_world_epoch()')
baseline = baseline.replace('        if(!transaction.finish(cause))return false;\n', '')
# The baseline hash pins the unchanged local ordered branch from ea90ba6.
expected = 'd9155dfc2bd86b20f54896e9f2801f456952984acc9acf531d263581913cd403'
assert hashlib.sha256(baseline.encode()).hexdigest() == expected, 'Local parent changed beyond explicit transaction routing'
(args.output / 'ordered_local_parent.inc').write_text(
    baseline.replace('ordered_local_context(', 'baseline_ordered_local_context(', 1) + '\n' + parent + '\n', encoding='utf-8')
print('info: extracted actual local context parent and both epoch readers')
