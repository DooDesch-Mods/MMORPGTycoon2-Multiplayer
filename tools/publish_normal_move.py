"""Publish native normal Move evidence without hiding the observed local marker."""
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
identifier = 'af5e1c96-20fb-425c-a307-328e73ae0343'
proof = json.loads((ROOT / 'artifacts/acceptance' / identifier / 'summary.json').read_text(encoding='utf-8'))
if proof.get('passed') is not True or proof.get('state') != 'confirmed-normal-Move-two':
    raise RuntimeError('normal native Move proof is incomplete')
images = []
for job in proof['jobs']:
    if 'frame' not in job['step']:
        continue
    evidence = json.loads(Path(job['evidence_path']).read_text(encoding='utf-8'))
    call = next(call for call in evidence['calls'] if call['tool'] == 'game_capture_frame')
    source = Path(call['result']['structuredContent']['result']['path'])
    target = ROOT / '_drafts/media' / ('normal_move_' + job['peer'] + '.bmp')
    shutil.copy2(source, target)
    images.append(target.name)
if len(images) != 2:
    raise RuntimeError('both normal Move native frames required')
text = '''# Normale Move-Aktion auf zwei Instanzen

[Native Abnahme](../artifacts/acceptance/af5e1c96-20fb-425c-a307-328e73ae0343/summary.json) bestätigt je einen normalen Move von Client und Host. Beide nutzen den echten BuildingInfoWindow-Befehl und native MouseOver/Press/Release, danach normale GameUpdate-Erfassung und denselben Host-Controller.

Lauff278fe98-ec5d-46ca-9597-2166902f73c8, DLL5e99a2b119fbff039b7ec9fc0ce29ce9cb80ba0fb6071c66eb2133ade07a31da. Je eine Erfassung, eine Host-geordnete Ausführung pro Peer, gleiche UID1086, echter Undo, Host-ACK3 und keine Änderung von Geld94.600 oder20 Buchungsgruppen. Das tatsächliche eingerastete Ziel wird vollständig im Receipt gespeichert. [19 Runner-Regressionen](../tools/move_ui_fresh_tests/evidence/20261012T203259412523Z/summary.json).

Beide Abschlussbilder wurden betrachtet. Das Clientbild zeigt zusätzlich eine weiße Vorschau oder Markierung an der früheren Position. Der native Objektzustand ist bestätigt; Ursache und Lebenszyklus dieser lokalen Anzeige sind noch offen. Die Bilder ersetzen diese Einschränkung nicht durch einen Erfolgshinweis.

'''
for image in images:
    text += '![Normale Move-Aktion](media/' + image + ')\n\n'
(ROOT / '_drafts/normal_move_two.md').write_text(text, encoding='utf-8')
for name in ('_drafts/progress.html', 'tools/update_progress.py'):
    path = ROOT / name
    content = path.read_text(encoding='utf-8')
    content = content.replace('Normale Move-UI: Rasterprüfung korrigiert und gebaut, native Abnahme läuft.', 'Normale Move-Aktion beider Spieler nativ bestätigt; zusätzliche Client-Markierung wird geprüft. <a href="normal_move_two.md">Belege und Bilder</a>.')
    if '<section id="normal-native-move">' not in content:
        section='<section id="normal-native-move"><h2>Normales gemeinsames Verschieben</h2><p>Beide normalen Move-Aufträge bestätigt, lokale Client-Markierung offen. <a href="normal_move_two.md">Native Quittungen und Einschränkung</a>.</p>'
        for image in images:
            section+='<img src="media/'+image+'" alt="Normale Move-Aktion auf einer Testinstanz">'
        section+='</section>'
        content=content.replace('</main>',section+'</main>')
    path.write_text(content, encoding='utf-8')
print('info: normal native Move receipts and viewed frames published; local marker remains open')
