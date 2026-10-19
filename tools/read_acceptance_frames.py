"""List existing frame files from named acceptance checks."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('summary', type=Path)
parser.add_argument('--prefix', default='native-title-frame-')
args = parser.parse_args()
summary = json.loads(args.summary.read_text(encoding='utf-8'))
frames = []
for check in summary['checks']:
    if not check['check'].startswith(args.prefix):
        continue
    result = check['result']
    path = Path(result['path'])
    if not path.is_file():
        raise RuntimeError('confirmed frame file is absent: ' + str(path))
    frames.append({'check':check['check'], 'path':str(path), 'bytes':path.stat().st_size})
if not frames:
    raise RuntimeError('no matching completed frame check: ' + args.prefix)
print(json.dumps(frames, ensure_ascii=False))
