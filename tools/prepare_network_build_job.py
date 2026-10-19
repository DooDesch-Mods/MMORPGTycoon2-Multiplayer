"""Prepare the owned-runtime build and bounded network identity acceptance job."""
import json
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--transport-only', action='store_true')
parser.add_argument('--protocol-recovery', action='store_true')
parser.add_argument('--title-entry', action='store_true')
parser.add_argument('--worker-entry', action='store_true')
parser.add_argument('--worker-diagnostics', action='store_true')
args = parser.parse_args()
if sum((args.transport_only, args.protocol_recovery, args.title_entry, args.worker_entry, args.worker_diagnostics)) > 1:
    parser.error('Choose one bounded job mode')
prior = json.loads((ROOT / 'artifacts/jobs/build_capture_world_witness.json').read_text(encoding='utf-8'))
job = {'candidate': 'WRITER-HANDOFF-1-GLOBAL-ROOT-1-NET-ID-MANIFEST-1', 'steps': [] if args.transport_only or args.protocol_recovery else prior['steps']}
bounded_native = args.title_entry or args.worker_entry or args.worker_diagnostics
if bounded_native:
    job['candidate'] = 'NET-ID-CALLER-CONTEXT-DIAGNOSTICS-1' if args.worker_diagnostics else 'NET-ID-LOAD-WORKER-1' if args.worker_entry else 'NET-ID-TITLE-ENTRY-1'
    job['steps'] = job['steps'][:7]
if args.protocol_recovery:
    for mode in ('data_idle', 'retired_replay'):
        job['steps'].append({'name': f'network-loopback-{mode}', 'cwd': str(ROOT), 'argv': [
            'python', '-X', 'utf8', 'test/snapshot_stream_test.py', '--size-mib', '1', '--mode', mode, '--timeout-seconds', '30']})
for variant in (() if bounded_native else ('fileonly', 'armfail', 'digestwrong') if args.protocol_recovery else ('confirmed', 'fileonly', 'armfail', 'digestwrong')):
    job['steps'].append({'name': f'network-loopback-{variant}', 'cwd': str(ROOT), 'argv': [
        'python', '-X', 'utf8', 'test/snapshot_stream_test.py', '--size-mib', '1',
        '--mode', 'all' if variant == 'confirmed' else 'valid', '--network-map', variant]})
if not bounded_native:
    job['steps'].append({'name': 'network-loopback-four-peers', 'cwd': str(ROOT), 'argv': [
        'python', '-X', 'utf8', 'test/snapshot_stream_test.py', '--size-mib', '1', '--mode', 'valid', '--players', '4']})
target = ROOT / 'artifacts/jobs' / ('network_identity_worker_diagnostics.json' if args.worker_diagnostics else 'network_identity_worker_entry.json' if args.worker_entry else 'network_identity_title_entry.json' if args.title_entry else 'network_identity_recovery.json' if args.protocol_recovery else 'network_identity_transport.json' if args.transport_only else 'network_identity_integration.json')
target.write_text(json.dumps(job, indent=2) + '\n', encoding='utf-8')
print(f'info: prepared candidate={job["candidate"]} steps={len(job["steps"])} path={target}')
