"""Retain an explicit annotation correction without replacing original evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import uuid

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--acceptance', type=lambda value: str(uuid.UUID(value)), required=True)
parser.add_argument('--scope', choices=['native', 'time'], required=True)
parser.add_argument('--frame-check', choices=['frame', 'paused_frame'], default='frame')
parser.add_argument('--visual-note', required=True)
args = parser.parse_args()
directory = (ROOT / 'artifacts/acceptance' if args.scope == 'native' else ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/runs') / args.acceptance
source = directory / (args.frame_check + '_checkpoint.json')
original = json.loads(source.read_bytes())
target = directory / (args.frame_check + '_visual_correction.json')
if target.exists():
    raise RuntimeError('visual correction is immutable; no original evidence replaced')
for role, frame in original['frames'].items():
    if hashlib.sha256(Path(frame['retained_path']).read_bytes()).hexdigest() != frame['file_sha256']:
        raise RuntimeError('retained visual source differs: ' + role)
record = {'acceptance_id': args.acceptance, 'checkpoint_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
          'original_visual_finding': original['visual_finding'], 'visual_finding': args.visual_note,
          'reason': 'Authored annotation corrected from inspected retained native pixels', 'game_operations': 0}
target.write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('info: explicit visual correction retained: ' + str(target))
