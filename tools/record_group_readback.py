"""Retain an acknowledged all-peer readback without operating the game."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import uuid

ROOT = Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--acceptance', required=True)
    args = parser.parse_args()
    identifier = str(uuid.UUID(args.acceptance))
    directory = ROOT / 'artifacts/acceptance' / identifier
    source = directory / 'summary.json'
    summary = json.loads(source.read_text(encoding='utf-8'))
    assert summary['passed'] and all(summary['proof'].values()), 'All-peer proof incomplete'
    count = summary['parameters']['players']
    assert count in (2, 3, 4)
    peers = ['host'] + [f'client-{n}' for n in range(1, count)]
    checks = {entry['check']: entry['result'] for entry in summary['checks']}
    frames = checks['all-peer-native-frames']
    assert set(frames) == set(peers), 'Frame peers incomplete'
    target = directory / 'group_checkpoint.json'
    assert not target.exists(), 'Checkpoint must stay immutable'
    retained = {}
    for peer in peers:
        frame = frames[peer]
        image = Path(frame['path']).resolve()
        image.relative_to((ROOT / '_MPTest' / peer / 'diagnostics').resolve())
        assert image.suffix == '.bmp' and image.stat().st_size == frame['bytes']
        destination = ROOT / '_drafts/media' / f'native_{identifier}_{peer}.bmp'
        assert not destination.exists(), 'Frame must stay immutable'
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(image, destination)
        assert digest(destination) == digest(image), 'Retained frame digest differs'
        retained[peer] = {'frame': frame, 'retained_path': str(destination), 'file_sha256': digest(destination)}
    record = {'schema_version': 1, 'acceptance_id': identifier,
              'source_acceptance_id': summary['source_acceptance'],
              'runtime_run_id': summary['runtime_run_id'],
              'summary_sha256': digest(source), 'proof': summary['proof'],
              'network_identity': checks['all-peer-native-network-identity'],
              'frames': retained, 'game_operations': 0, 'full_coop_proven': False}
    target.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(f'info: all-peer checkpoint peers={count} target={target} game_operations=0')

if __name__ == '__main__':
    main()
