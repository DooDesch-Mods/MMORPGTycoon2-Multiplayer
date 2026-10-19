"""Retain exact native acceptance frames with the observer's explicit visual finding."""
import argparse
import hashlib
import json
from pathlib import Path
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--acceptance', required=True)
parser.add_argument('--visual-note', required=True)
parser.add_argument('--phase', choices=('single', 'before', 'after'), default='single')
args = parser.parse_args()
identifier = str(uuid.UUID(args.acceptance))
directory = ROOT / 'artifacts/acceptance' / identifier
source = directory / 'summary.json'
summary = json.loads(source.read_text(encoding='utf-8'))
frames = [entry['result'] for entry in summary['checks'] if entry['check'] == 'native-frame-artifact']
if args.phase != 'single':
    grouped = {}
    for frame in frames:
        grouped.setdefault(frame['peer'], []).append(frame)
    if not grouped or any(len(values) != 2 for values in grouped.values()):
        raise RuntimeError('before/after native frame phases require exactly two frames per peer')
    frames = [values[0 if args.phase == 'before' else 1] for values in grouped.values()]
if not frames or len({frame['peer'] for frame in frames}) != len(frames):
    raise RuntimeError('native acceptance frame set is absent or duplicated')
suffix = '' if args.phase == 'single' else '_' + args.phase
checkpoint = directory / f'frame_checkpoint{suffix}.json'
if checkpoint.exists():
    raise RuntimeError('native frame checkpoint is immutable')
retained = {}
for frame in frames:
    path = Path(frame['path']).resolve()
    path.relative_to((ROOT / '_MPTest' / frame['peer'] / 'diagnostics').resolve())
    payload = path.read_bytes()
    digest = hashlib.sha256(payload).hexdigest()
    if len(payload) != frame['bytes'] or digest != frame['file_sha256']:
        raise RuntimeError(f'native frame source differs: {path}')
    destination = ROOT / '_drafts/media' / f'native_{identifier}{suffix}_{frame["peer"]}.bmp'
    if destination.exists():
        raise RuntimeError(f'native retained frame already exists: {destination}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    if hashlib.sha256(destination.read_bytes()).hexdigest() != digest:
        raise RuntimeError(f'native retained frame write differs: {destination}')
    retained[frame['peer']] = {**frame, 'retained_path': str(destination)}
record = {'acceptance_id': identifier, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
          'frame_phase': args.phase,
          'visual_finding': args.visual_note, 'frames': retained, 'game_operations': 0, 'full_coop_proven': False}
checkpoint.write_text(json.dumps(record, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print(f'info: native frames retained={len(retained)} checkpoint={checkpoint}; no game effects')
