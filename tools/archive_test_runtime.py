"""Retain immutable log prefixes and manifests from one owned local runtime."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/mcp'))
from runtime import shared_open, atomic_json, read_json


def main():
    runtime = (ROOT / '_MPTest').resolve()
    run = read_json(runtime / 'run.json')
    run_id = str(uuid.UUID(run['run_id']))
    if run.get('owner') != {'kind': 'MPTest', 'run_id': run_id} or Path(run['test_root']).resolve() != runtime:
        raise RuntimeError('runtime archive ownership or root differs')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    target = ROOT / 'artifacts/runtime_archive' / run_id / stamp
    target.mkdir(parents=True, exist_ok=False)
    receipt = {'run_id': run_id, 'utc_time': datetime.now(timezone.utc).isoformat(), 'passed': False,
               'scope': 'owned manifests and captured log prefixes; no save files', 'files': [], 'skips': []}
    sources = [(runtime / 'run.json', Path('run.json'))]
    for record in run['processes']:
        if record.get('owner') != run['owner'] or record.get('run_id') != run_id:
            raise RuntimeError(f"runtime archive process owner differs role={record.get('role')}")
        instance = Path(record['runtime_root']).resolve()
        if instance.parent != runtime or instance.name not in {'host', 'client-1', 'client-2', 'client-3'}:
            raise RuntimeError(f'runtime archive instance root differs: {instance}')
        log = Path(record['log_path']).resolve()
        if log.parent != instance / 'logs':
            raise RuntimeError(f'runtime archive process log escapes instance: {log}')
        sources.append((log, log.relative_to(runtime)))
        if record['role'] == 'game':
            sources.append((instance / 'instance.json', Path(instance.name) / 'instance.json'))
            sources.append((instance / 'copy-manifest.json', Path(instance.name) / 'copy-manifest.json'))
            for name in ('mt2mp.log', 'mt2mp_proxy_debug.log', 'MT2.RPT'):
                sources.append((instance / 'game' / name, Path(instance.name) / 'game' / name))
    try:
        for source, relative in sources:
            source = source.resolve()
            if not source.is_relative_to(runtime):
                raise RuntimeError(f'runtime archive source escapes owned root: {source}')
            if source.name == 'MT2.RPT' and not source.exists():
                receipt['skips'].append({'source': str(source), 'reason': 'optional native report was not created'})
                print(f'info: runtime archive skip source={source} reason=optional native report absent')
                continue
            output = target / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            digest = hashlib.sha256()
            with shared_open(source) as incoming, output.open('xb') as outgoing:
                remaining = length = os.fstat(incoming.fileno()).st_size
                while remaining:
                    block = incoming.read(min(remaining, 512 * 1024))
                    if not block:
                        raise RuntimeError(f'runtime archive source truncated path={source} remaining={remaining}')
                    outgoing.write(block)
                    digest.update(block)
                    remaining -= len(block)
                outgoing.flush()
                os.fsync(outgoing.fileno())
            with output.open('rb') as saved:
                actual = hashlib.file_digest(saved, 'sha256').hexdigest()
            if output.stat().st_size != length or actual != digest.hexdigest():
                raise RuntimeError(f'runtime archive readback differs source={source} target={output}')
            receipt['files'].append({'source': str(source), 'path': str(output), 'bytes': length,
                                     'sha256': digest.hexdigest(), 'capture': 'initial-length prefix'})
        receipt['passed'] = True
    except Exception as error:
        receipt['error'] = {'operation': 'archive-owned-runtime', 'source': str(source), 'cause': f'{type(error).__name__}: {error}'}
        print(f"error: runtime archive run={run_id} cause={receipt['error']['cause']}", file=sys.stderr)
    atomic_json(target / 'receipt.json', receipt)
    print(json.dumps({'passed': receipt['passed'], 'receipt': str(target / 'receipt.json'), 'files': len(receipt['files'])}))
    return 0 if receipt['passed'] else 1


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as error:
        print(f'error: runtime archive startup: {type(error).__name__}: {error}', file=sys.stderr)
        sys.exit(1)
