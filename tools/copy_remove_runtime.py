"""Import the sealed RemoveRuntime after the actual two-peer native Free gate."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'tools/engine_analysis/object_identity/build_actions/remove_runtime'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    frozen = json.loads((BASE / 'freeze.json').read_text(encoding='utf-8'))
    manifest_path = BASE / 'production_copy_manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    proof = ROOT / 'artifacts/acceptance/0e2cf353-0098-4b56-bef6-3fadbfa363cc/native_free_checkpoint.json'
    native = json.loads(proof.read_text(encoding='utf-8'))
    if frozen.get('passed') is not True or native.get('passed') is not True or native.get('holders_confirmed') != 4:
        raise ValueError('sealed Remove candidate or actual native Free gate absent')
    for path, expected in manifest['canonical_headers'].items():
        if sha(ROOT / path) != expected:
            raise ValueError('canonical Free header differs: ' + path)
    if sha(ROOT / manifest['required_FreeRuntime_freeze']) != manifest['required_FreeRuntime_freeze_sha256']:
        raise ValueError('required FreeRuntime freeze changed')
    copies = []
    for entry in manifest['copy_sources']:
        source, staged = ROOT / entry['source'], ROOT / entry['staged']
        target = (ROOT / entry['target']).resolve()
        if not target.is_relative_to(ROOT / 'src/game/object_identity/remove_runtime'):
            raise ValueError('Remove target outside private hierarchy')
        if sha(source) != entry['source_sha256'] or sha(staged) != entry['target_sha256']:
            raise ValueError('Remove candidate copy hash changed: ' + str(source))
        if target.exists() and sha(target) != entry['target_sha256']:
            raise ValueError('existing Remove production target differs: ' + str(target))
        copies.append((source, staged, target, entry['target_sha256']))
    if len(copies) != 34 or len({item[2] for item in copies}) != 34:
        raise ValueError('Remove copy coverage differs from sealed candidate')
    directory = ROOT / 'artifacts/remove_integration' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    directory.mkdir(parents=True, exist_ok=False)
    receipt = {'passed': False, 'candidate': manifest['candidate'], 'freeze_sha256': sha(BASE / 'freeze.json'),
               'manifest_sha256': sha(manifest_path), 'native_free_checkpoint_sha256': sha(proof), 'copies': []}
    try:
        for source, staged, target, expected in copies:
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                with target.open('xb') as stream:
                    stream.write(staged.read_bytes())
            actual = sha(target)
            receipt['copies'].append({'source': str(source), 'staged': str(staged), 'target': str(target),
                                      'target_sha256': actual, 'confirmed': actual == expected})
            if actual != expected:
                raise ValueError('Remove production hash mismatch: ' + str(target))
        receipt['passed'] = True
    except Exception as failure:
        receipt['error'] = f'{type(failure).__name__}: {failure}'
        raise
    finally:
        output = directory / 'copy_receipt.json'
        output.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
        print(f'info: Remove copy passed={receipt["passed"]} files={len(receipt["copies"])} receipt={output}')


if __name__ == '__main__':
    main()
