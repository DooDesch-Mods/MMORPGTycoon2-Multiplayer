"""Recover a prior-process MCP lock, then load the already restarted title once."""
from pathlib import Path
import json
import shutil
import sys
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log
from argparse import Namespace

args = Namespace(test_root=ROOT / '_MPTest', players=2, cleanup=False, step_timeout=45)
runner = AcceptanceRunner(args)
try:
    previous = read_json(ROOT / 'artifacts/acceptance/ee7ec5ce-6bf3-4692-9e30-822d92476d7e/summary.json')
    if previous['step'] != 'title-host-01' or any(job['step'] == 'host-load-once' for job in previous['jobs']):
        raise AcceptanceError('previous setup could have attempted load; no replay')
    runner.load_manifests()
    if runner.summary['runtime_run_id'] != '089524cf-1224-45dc-a693-03a5bafebc42':
        raise AcceptanceError('restarted runtime differs')
    retained = ROOT / 'test/mcp/evidence/prior-placement-03d0/instance.json'
    retained.parent.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'artifacts/placement_failure/20261012T175444496248Z/_MPTest/host/instance.json'
    shutil.copy2(source, retained)
    spec = runner.directory / 'recover.json'
    spec.write_text(json.dumps({'calls': [{'tool': 'recover_pending', 'arguments': {'previous_manifest_path': str(retained)}}]}))
    runner.command('confirm-exit-and-recover-old-mailbox', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/mcp/run_job.py'), '--manifest', str(ROOT / '_MPTest/host/instance.json'), '--spec', str(spec), '--label', 'recover-placement-lock'])
    runner.command('load-current-title-once', [sys.executable, '-X', 'utf8', str(ROOT / 'tools/native_open_acceptance.py'),
        '--reuse-run-id', runner.summary['runtime_run_id'], '--snapshot-id', '41f6e0b5-bcc2-4ef4-b930-ddfe6b3dffe2',
        '--expected-input-bytes', '208949', '--expected-input-sha256', 'fedec7bec52be5c40a362e0552390e804bbe86defc949524c6f9c4a20a519c1a',
        '--expected-dll-sha256', previous['parameters']['expected_dll_sha256'], '--players', '2'])
    runner.summary.update(passed=True, state='recovered-owned-placement-fixture', new_run_id=runner.summary['runtime_run_id'])
except Exception as error:
    runner.summary.update(passed=False, state='failed-placement-recovery-resume', error={'step': runner.summary['step'], 'cause': str(error), 'behavior': 'remaining mutations skipped; no replay'})
    log('error', 'placement recovery resume: ' + str(error))
runner.summary['finished_utc'] = utc(); runner.persist()
print(str(runner.summary_path))
raise SystemExit(0 if runner.summary['passed'] else 1)
