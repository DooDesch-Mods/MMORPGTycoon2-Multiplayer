"""Import only sealed retirement updates over the exact parent sources."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'tools/engine_analysis/object_identity/build_actions/remove_runtime'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    delta = BASE / 'retirement_delta'
    frozen = json.loads((delta / 'freeze.json').read_text(encoding='utf-8'))
    manifest = json.loads((delta / 'production_copy_manifest.json').read_text(encoding='utf-8'))
    parent = json.loads((BASE / 'production_copy_manifest.json').read_text(encoding='utf-8'))
    expected = {item['target']: item['target_sha256'] for item in parent['copy_sources']}
    proof = ROOT / 'artifacts/acceptance/c71af124-2a7a-4c3f-9313-f73187ee8eb3/native_remove_checkpoint.json'
    if frozen.get('passed') is not True or json.loads(proof.read_text(encoding='utf-8')).get('passed') is not True:
        raise ValueError('sealed retirement or actual two-peer removal gate absent')
    updates = manifest['additive_updates']
    if len(updates) != 12:
        raise ValueError('retirement update coverage differs')
    for item in updates:
        target = ROOT / item['target']
        if sha(ROOT / item['source']) != item['source_sha256'] or sha(ROOT / item['staged']) != item['target_sha256']:
            raise ValueError('retirement staged hash changed: ' + str(target))
        if item['target'] in expected:
            if sha(target) != expected[item['target']]:
                raise ValueError('retirement parent differs: ' + str(target))
        elif target.exists():
            raise ValueError('new retirement target already exists: ' + str(target))
    directory = ROOT / 'artifacts/remove_retirement_integration' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    directory.mkdir(parents=True, exist_ok=False)
    receipt = {'passed': False, 'manifest_sha256': sha(delta / 'production_copy_manifest.json'), 'copies': []}
    try:
        for item in updates:
            target = ROOT / item['target']
            target.write_bytes((ROOT / item['staged']).read_bytes())
            actual = sha(target)
            receipt['copies'].append({'target': str(target), 'sha256': actual})
            if actual != item['target_sha256']:
                raise ValueError('retirement copied hash differs: ' + str(target))
        receipt['passed'] = True
    except Exception as error:
        receipt['error'] = str(error)
        raise
    finally:
        output = directory / 'copy_receipt.json'
        output.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
        print(f'info: retirement copy passed={receipt["passed"]} files={len(receipt["copies"])} receipt={output}')

if __name__ == '__main__':
    main()
