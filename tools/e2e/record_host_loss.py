"""Preserve the inspected native Host-loss frame and its immutable source hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--summary', type=Path, required=True)
args = parser.parse_args()
raw = args.summary.read_bytes()
summary = json.loads(raw)
if summary.get('passed') is not True or summary.get('host_termination', {}).get('state') != 'confirmed':
    raise RuntimeError('Host-loss native execution is not confirmed')
frames = [check['result'] for check in summary['checks'] if check['check'] == 'native-frame-artifact']
if len(frames) != 1 or frames[0]['peer'] != 'client-1':
    raise RuntimeError('Host-loss exact Client frame missing')
frame = frames[0]
source = Path(frame['path']).resolve()
if not source.is_relative_to((ROOT / '_MPTest/client-1/diagnostics').resolve()):
    raise RuntimeError('Host-loss frame outside owned Client diagnostics')
if hashlib.sha256(source.read_bytes()).hexdigest() != frame['file_sha256']:
    raise RuntimeError('Host-loss native frame hash differs')
destination = ROOT / '_drafts/media/host_loss_client.bmp'
destination.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(source, destination)
if hashlib.sha256(destination.read_bytes()).hexdigest() != frame['file_sha256']:
    raise RuntimeError('Host-loss preserved frame readback differs')
report = {'state': 'confirmed-native-local-host-loss', 'source_summary': str(args.summary.resolve()),
          'source_summary_sha256': hashlib.sha256(raw).hexdigest(), 'run_id': summary['runtime_run_id'],
          'host_termination': summary['host_termination'], 'media': str(destination),
          'frame_sha256': frame['file_sha256'],
          'visual_readback': 'Inspected native frame: Sitzung beendet: Host nicht erreichbar; Verbundene Gegenstellen: 0; overlay visible; game remains PAUSED',
          'scope_limit': 'two local peers; no physical Steam transport acceptance'}
path = ROOT / '_drafts/host_loss_native.json'
path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(path)
