"""Copy a sealed native Free candidate into the production hierarchy with hash receipts."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    manifest = args.manifest.resolve(strict=True)
    source = json.loads(manifest.read_text(encoding='utf-8'))
    if source.get('passed') is not True or not source.get('copy_sources'):
        raise ValueError('sealed copy manifest lacks passed evidence or copy_sources')
    records = []
    for entry in source['copy_sources']:
        path = (ROOT / entry['source']).resolve(strict=True)
        target = (ROOT / entry['target']).resolve()
        if not path.is_relative_to(ROOT) or not target.is_relative_to(ROOT / 'src/game/object_identity'):
            raise ValueError(f'copy path outside candidate hierarchy: {path} -> {target}')
        actual = sha(path)
        if actual != entry['sha256']:
            raise ValueError(f'candidate hash differs: {path} actual={actual} required={entry["sha256"]}')
        if target.exists() and sha(target) != actual:
            raise ValueError(f'production target differs; no overwrite: {target}')
        records.append((path, target, actual))
    if len({target for _, target, _ in records}) != len(records):
        raise ValueError('duplicate production copy target')
    directory = ROOT / 'artifacts/native_free_integration' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    directory.mkdir(parents=True, exist_ok=False)
    receipt = {'candidate': source.get('candidate'), 'manifest': str(manifest), 'manifest_sha256': sha(manifest),
               'passed': False, 'copies': [], 'native_game_proven': False}
    output = directory / 'copy_receipt.json'
    try:
        for path, target, expected in records:
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                with target.open('xb') as stream:
                    stream.write(path.read_bytes())
            actual = sha(target)
            receipt['copies'].append({'source': str(path), 'target': str(target), 'source_sha256': expected,
                                      'target_sha256': actual, 'confirmed': actual == expected})
            if actual != expected:
                raise ValueError(f'production copy hash mismatch: {target}')
        receipt['passed'] = True
    except Exception as failure:
        receipt['error'] = f'{type(failure).__name__}: {failure}'
        raise
    finally:
        output.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
        print(f'info: copy passed={receipt["passed"]} files={len(receipt["copies"])} receipt={output}')


if __name__ == '__main__':
    main()
