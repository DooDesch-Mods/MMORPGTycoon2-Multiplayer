"""Retain native time receipts and BMP files without calling the game."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--acceptance', required=True)
    parser.add_argument('--visual-note', required=True)
    parser.add_argument('--frame-check', choices=['frame', 'paused_frame'], default='frame')
    args = parser.parse_args()
    identifier = str(uuid.UUID(args.acceptance))
    directory = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/runs' / identifier
    source = directory / 'summary.json'
    data = source.read_bytes()
    summary = json.loads(data)
    if summary.get('passed') is not True or summary.get('native_runtime_proof') is not True:
        raise RuntimeError('native time acceptance is not confirmed')
    checkpoint = directory / (args.frame_check + '_checkpoint.json')
    if checkpoint.exists():
        raise RuntimeError('native frame checkpoint is immutable')
    confirmed = [entry['result'] for entry in summary['checks'] if entry['check'] == 'input_confirmed']
    tokens = [entry['token'] for entry in confirmed]
    if not tokens or len(set(tokens)) != len(tokens):
        raise RuntimeError('native input receipt set absent or duplicated')
    receipts = []
    for entry in confirmed:
        native = entry['native_input']
        if entry['state'] != 'confirmed' or native['state'] != 'confirmed' or native['hook']['captured_count'] != 1:
            raise RuntimeError(f'native input is not confirmed: {entry["token"]}')
        receipts.append({key: entry[key] for key in ('token', 'peer', 'key', 'cleanup')})
        receipts[-1]['native_input'] = native
    frames = [entry['result'] for entry in summary['checks'] if entry['check'] == args.frame_check]
    if not frames or len({entry['peer'] for entry in frames}) != len(frames):
        raise RuntimeError('native frame set absent or duplicated')
    prepared = []
    for entry in frames:
        peer = entry['peer']
        role = 'host' if peer == 1 else f'client-{peer - 1}'
        frame = entry['result']
        path = Path(frame['path']).resolve()
        path.relative_to((ROOT / '_MPTest' / role / 'diagnostics').resolve())
        payload = path.read_bytes()
        if payload[:2] != b'BM' or len(payload) != frame['bytes']:
            raise RuntimeError(f'native BMP size/header differs: {path}')
        offset = struct.unpack_from('<I', payload, 10)[0]
        if offset >= len(payload) or hashlib.sha256(payload[offset:]).hexdigest() != frame['pixel_sha256']:
            raise RuntimeError(f'native BMP pixels differ: {path}')
        suffix = '_paused' if args.frame_check == 'paused_frame' else ''
        destination = ROOT / '_drafts/media' / f'native_{identifier}{suffix}_{role}.bmp'
        if destination.exists():
            raise RuntimeError(f'retained frame already exists: {destination}')
        prepared.append((role, frame, destination, payload))
    retained = {}
    for role, frame, destination, payload in prepared:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(payload)
        digest = hashlib.sha256(payload).hexdigest()
        if hashlib.sha256(destination.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'retained frame write differs: {destination}')
        retained[role] = {**frame, 'file_sha256': digest, 'retained_path': str(destination)}
    record = {'acceptance_id': identifier, 'source_sha256': hashlib.sha256(data).hexdigest(),
              'runtime_run_id': summary['runtime_run_id'], 'source_scope': summary['scope'],
              'scope': 'native_keyboard_receipts_and_retained_frames',
              'shared_hud_readback_proven': summary.get('shared_hud_readback_proven', False),
              'calendar_formatter_readback_proven': summary.get('calendar_formatter_readback_proven', False),
              'native_receipts': receipts, 'Space_proven': summary['Space_proven'],
              'visual_finding': args.visual_note, 'frames': retained,
              'game_operations': 0, 'calendar_authority_proven': False, 'full_coop_proven': False}
    checkpoint.write_text(json.dumps(record, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(f'info: retained native inputs={len(receipts)} frames={len(retained)} checkpoint={checkpoint}; no game effects')


if __name__ == '__main__':
    main()
