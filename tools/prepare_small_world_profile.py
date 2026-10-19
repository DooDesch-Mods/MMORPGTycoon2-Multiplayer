"""Skip the introductory tutorial in stopped, owned MPTest profiles only."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import uuid
from local_test import WindowsProcesses

ROOT = Path(__file__).resolve().parents[1]

def prepare(test_root):
    root = Path(test_root).resolve()
    if root != (ROOT / '_MPTest').resolve():
        raise RuntimeError('small profile setup requires the exact owned workspace _MPTest')
    plans = []
    processes = WindowsProcesses()
    for peer in ('host', 'client-1'):
        manifest = json.loads((root / peer / 'instance.json').read_text(encoding='utf-8'))
        if manifest.get('owner') != {'kind': 'MPTest', 'run_id': manifest.get('run_id')}:
            raise RuntimeError(f'small profile setup owner mismatch: {peer}')
        state = processes.inspect(manifest)
        if not state.startswith('exited'):
            raise RuntimeError(f'small profile setup requires stopped own process peer={peer} actual={state}')
        paths = list((root / peer / 'data').glob('*/metagame.vrt'))
        if len(paths) != 1:
            raise RuntimeError(f'small profile metagame path count peer={peer} actual={len(paths)} required=1')
        path = paths[0].resolve()
        path.relative_to(root / peer / 'data')
        before = path.read_bytes()
        text = before.decode('utf-8')
        if len(re.findall(r'finishedTutorial "(?:false|true)"', text)) != 1:
            raise RuntimeError(f'small profile tutorial flag is not unique: {path}')
        after = re.sub(r'finishedTutorial "(?:false|true)"', 'finishedTutorial "true"', text).encode('utf-8')
        plans.append((peer, path, before, after))
    evidence = ROOT / 'artifacts/test_setup' / str(uuid.uuid4())
    evidence.mkdir(parents=True, exist_ok=False)
    records = []
    for peer, path, before, after in plans:
        backup = evidence / f'{peer}_metagame.vrt'
        backup.write_bytes(before)
        path.write_bytes(after)
        if path.read_bytes() != after:
            raise RuntimeError(f'small profile tutorial setting write differs: {path}')
        records.append({'peer': peer, 'path': str(path), 'backup': str(backup),
                        'before_sha256': hashlib.sha256(before).hexdigest(),
                        'after_sha256': hashlib.sha256(after).hexdigest(), 'changed': before != after})
    receipt = evidence / 'summary.json'
    receipt.write_text(json.dumps({'state': 'confirmed-file-write', 'engine_effect': 'unverified', 'profiles': records}, indent=2) + '\n', encoding='utf-8')
    print(f'info: small owned tutorial flags written; engine effect unverified; receipt={receipt}', flush=True)
    return receipt

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    prepare(parser.parse_args().test_root)
