"""Publish measured two-peer original construction and retained framebuffer evidence."""
from pathlib import Path
import json
import shutil
ROOT = Path(__file__).resolve().parents[1]
summary_path = ROOT / 'artifacts/acceptance/9bacb10f-14f0-4017-a641-06c45103b2f0/summary.json'
summary = json.loads(summary_path.read_text())
assert summary['passed'] is True and summary['state'] == 'confirmed-original-building-two'
media = ROOT / '_drafts/media'
media.mkdir(exist_ok=True)
images = []
for job in summary['jobs']:
    if job['step'].startswith('host-frame-'):
        evidence = json.loads(Path(job['evidence_path']).read_text())
        frame = evidence['calls'][-1]['result']['structuredContent']['result']
        peer = job['step'].removeprefix('host-frame-')
        target = media / ('original_build_two_' + peer + '.bmp')
        shutil.copy2(frame['path'], target)
        images.append(target.name)
text = '''# Originaler Bauweg auf zwei Instanzen

[Native Abnahme](../artifacts/acceptance/9bacb10f-14f0-4017-a641-06c45103b2f0/summary.json) besteht auf DLL313d129d246ba79e2746b3de58d14f7f7d8f4ccd85416ca2f676bbd55c81c188, Lauf8c733523-726a-4af7-a07f-664681553b4b.

Client und Host haben jeweils einen neuen SmallInn/base-Auftrag über denselben Host-geordneten Controller erzeugt. Auf jedem Peer lief der originale Build.Update mit Purchase, AddBuilding und AddUndo genau einmal pro Auftrag. Kosten400 pro Gebäude, Geld94600 auf94200 und danach93800, alle20 Buchungsgruppen gleich. Der native Undo-Graph und private D0-Abschluss sind bestätigt, Host-ACK-Maske3. Gemeinsame IDs sind action:84fda31c-0991-41da-8504-f839b8724edc und action:73e23f9d-45c9-4b00-ae1b-f24e385568c4.

Die vier MCP-Framebuffer wurden betrachtet. Die Abschlussbilder zeigen drei Gebäude einschließlich des alten Baseline-Gasthauses und Geld93800 auf beiden Instanzen. Die Aufträge wurden direkt über den MCP-Controller eingereicht; normale Mausplatzierung ist separat noch offen.

Die Höhe kann beim folgenden nativen Welt-Update weiter an den Boden angepasst werden. Der anschließende [Remove-Versuch](../artifacts/acceptance/6df12fae-3ad9-487c-8738-e997583b6dbc/summary.json) wurde vor der Aktionsaufnahme wegen einer historischen Registryhöhe abgewiesen. Die gemeinsame ID muss ihre aktuelle Höhe aus dem echten Gebäude lesen. Keine Löschung oder Replay dieses Probeauftrags.

[16 gezielte Höhenregressionen](../tools/placement_height_tests/evidence/sealed_20261012T182112987328Z/summary.json) bestehen. Frühere Zeit-, Remove- und Persistenzbelege werden übernommen. Verschieben, normale Platzierung, weitere Objektarten und laufende Simulation bleiben offen.

'''
for name in images:
    text += '![Originaler Bau auf zwei Instanzen](media/' + name + ')\n\n'
(ROOT / '_drafts/original_build_two.md').write_text(text, encoding='utf-8')
notice = '<p class="pass"><strong>Originaler Bauweg auf zwei Instanzen bestanden:</strong>'
replacement = '<p class="pass"><strong>Originaler Bauweg auf zwei Instanzen bestanden:</strong> Je ein Client- und Host-Auftrag erzeugt SmallInn mit nativen Kosten und Undo einmal pro Peer. Geld93.800 und Buchungsgruppen stimmen überein. <a href="original_build_two.md">Beleg und native Bilder</a>. Normale Mausplatzierung bleibt offen; die aktuelle Bodenhöhe wird für das Entfernen neuer Objekte ergänzt.</p>'
for name in ('_drafts/progress.html', 'tools/update_progress.py'):
    path = ROOT / name; value = path.read_text(encoding='utf-8')
    start = value.index(notice); end = value.index('</p>', start) + 4
    value = value[:start] + replacement + value[end:]
    section = '<section><h2>Originaler gemeinsamer Bau</h2><p>Native MCP-Bilder nach Client- und Host-Bau, jeweils einmal pro Peer.</p>'
    for image in images:
        section += '<img src="media/' + image + '" alt="Kleine Welt mit drei Gasthäusern und Geld93.800">'
    section += '</section>'
    if '<h2>Originaler gemeinsamer Bau</h2>' not in value:
        value = value.replace('</main>', section + '</main>')
    path.write_text(value, encoding='utf-8')
print('info: original build proof and native progress media published')
