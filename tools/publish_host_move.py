"""Publish the retained Host Move evidence and already inspected native images."""
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
identifier = '398c51de-dc69-4d6a-87fc-d947b0608138'
proof = json.loads((ROOT / 'artifacts/acceptance' / identifier / 'summary.json').read_text(encoding='utf-8'))
if proof.get('passed') is not True:
    raise RuntimeError('native Host Move proof failed')
images = []
for job in proof['jobs']:
    if 'frame' not in job['step']:
        continue
    evidence = json.loads(Path(job['evidence_path']).read_text(encoding='utf-8'))
    call = next(call for call in evidence['calls'] if call['tool'] == 'game_capture_frame')
    source = Path(call['result']['structuredContent']['result']['path'])
    target = ROOT / '_drafts/media' / ('host_move_' + job['peer'] + '.bmp')
    shutil.copy2(source, target)
    images.append(target.name)
if len(images) != 2:
    raise RuntimeError('both native Host Move frames required')
text = '''# Originaler Host-Move auf zwei Instanzen

[Native Abnahme](../artifacts/acceptance/398c51de-dc69-4d6a-87fc-d947b0608138/summary.json) bestätigt den Host-Auftrag auf beiden Instanzen. Lauf421e214f-ac70-4b56-951f-c6c8c305d6dc, DLL8bf8474b75c4cbb6db270f8fd8e8103213d2660861cbdccfb6a5ba50f9ae1906.

SmallInn/base behält UID1086. Beide Instanzen zeigen dieselbe Position7990/63,5814094543457/8550, Geld94.600 und20 unveränderte Buchungsgruppen. Originaler DoMove, echter inverser Undo, Aufräumen und Host-ACK3 sind bestätigt. Beide nativen Bilder wurden betrachtet.

Die normalen Move-Werkzeuge sind eine separate, noch offene Abnahme. Die direkte Client-Abnahme bleibt [übernommen](client_move_two.md).

'''
for image in images:
    text += '![Host-Move auf beiden Instanzen](media/' + image + ')\n\n'
(ROOT / '_drafts/host_move_two.md').write_text(text, encoding='utf-8')
print('info: retained native Host Move and two viewed images published')
