"""Retain completed native MCP frames without issuing a game operation."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--acceptance', required=True)
parser.add_argument('--host-evidence', type=Path, required=True)
parser.add_argument('--client-evidence', type=Path, required=True)
args = parser.parse_args()
identifier = str(uuid.UUID(args.acceptance))
directory = ROOT / 'artifacts/acceptance' / identifier
checkpoint = json.loads((directory / 'native_identity_checkpoint.json').read_text(encoding='utf-8'))
target = directory / 'native_frame_checkpoint.json'
assert not target.exists(), 'Frame checkpoint must stay immutable'
frames = {}
for peer, evidence_arg in [('host', args.host_evidence), ('client-1', args.client_evidence)]:
    evidence_path = evidence_arg.resolve()
    evidence_path.relative_to(ROOT.resolve())
    evidence = json.loads(evidence_path.read_text(encoding='utf-8'))
    calls = [call for call in evidence['calls'] if call['tool'] == 'game_capture_frame']
    assert len(calls) == 1 and calls[0]['state'] == 'confirmed', 'MCP frame call lacks confirmed acknowledgement'
    response = calls[0]['result']['structuredContent']
    assert response['ok'] and response['run_id'] == checkpoint['runtime_run_id']
    frame = response['result']
    source = Path(frame['path']).resolve()
    source.relative_to((ROOT / '_MPTest' / peer / 'diagnostics').resolve())
    assert source.suffix == '.bmp' and source.stat().st_size == frame['bytes']
    destination = ROOT / '_drafts/media' / f'native_{identifier}_{peer}.bmp'
    assert not destination.exists(), 'Frame copy must stay immutable'
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    assert hashlib.sha256(destination.read_bytes()).hexdigest() == digest
    frames[peer] = {'evidence_path': str(evidence_path),
                    'evidence_sha256': hashlib.sha256(evidence_path.read_bytes()).hexdigest(),
                    'frame': frame, 'retained_path': str(destination), 'file_sha256': digest}
record = {'schema_version': 1, 'acceptance_id': identifier,
          'run_id': checkpoint['runtime_run_id'], 'frames': frames,
          'game_operations': 0, 'full_coop_proven': False}
target.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(f'info: frames retained=2 checkpoint={target} game_operations=0')
