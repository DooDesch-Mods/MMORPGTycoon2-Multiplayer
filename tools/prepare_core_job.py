"""Create an immutable core build job from the existing native checks."""
import argparse
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--candidate', required=True)
parser.add_argument('--name', required=True)
args = parser.parse_args()
if not args.name or any(char not in 'abcdefghijklmnopqrstuvwxyz0123456789_-' for char in args.name):
    raise RuntimeError('job name must be a simple lowercase filename')
job = json.loads((ROOT / 'artifacts/jobs/native_open_path.json').read_text(encoding='utf-8'))
job['candidate'] = args.candidate
path = ROOT / 'artifacts/jobs' / (args.name + '.json')
if path.exists():
    raise RuntimeError('build job already exists: ' + str(path))
path.write_text(json.dumps(job, indent=2) + '\n', encoding='utf-8')
print('info: immutable core job=' + str(path))
