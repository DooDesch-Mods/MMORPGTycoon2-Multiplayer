"""Prepare the source-build checks for the time Core integration."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
prior = json.loads((ROOT / 'artifacts/jobs/network_identity_worker_diagnostics.json').read_text(encoding='utf-8'))
job = {'candidate': 'TIME-INTEGRATION-1-CORE', 'steps': prior['steps'][2:7]}
target = ROOT / 'artifacts/jobs/time_core_integration.json'
assert not target.exists(), 'Build job must stay immutable'
target.write_text(json.dumps(job, indent=2) + '\n', encoding='utf-8')
print(f'info: prepared time source build checks steps={len(job["steps"])} job={target}')
