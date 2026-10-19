"""Publish the new native build/remove cycle and its inspected frames."""
from pathlib import Path
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]
place_id = 'd9a0f3e0-d2ff-406e-a33f-17352f9eaca7'
remove_id = '861f1ad9-e441-4095-95fd-f81869d41c76'
def proof(identifier, state):
    value = json.loads((ROOT / 'artifacts/acceptance' / identifier / 'summary.json').read_text(encoding='utf-8'))
    if value.get('passed') is not True or value.get('state') != state:
        raise RuntimeError('native acceptance is incomplete: ' + identifier)
    return value
place = proof(place_id, 'confirmed-normal-building-UI-two')
remove = proof(remove_id, 'confirmed-native-UI-remove-two')
if place['runtime_run_id'] != remove['runtime_run_id']:
    raise RuntimeError('build/remove runtime identity differs')
ledger = next(row['result'] for row in remove['checks'] if row['check'] == 'actual-UI-native-ledgers')
if ledger['cash'] != 94600:
    raise RuntimeError('native refund ledger differs')
images = []
for label, value in [('place', place), ('remove', remove)]:
    frames = [row['result'] for row in value['checks'] if row['check'] == 'native-frame-artifact']
    for peer in ('host', 'client-1'):
        frame = [row for row in frames if row['peer'] == peer][-1]
        source = Path(frame['path'])
        target = ROOT / '_drafts/media' / ('dynamic_' + label + '_' + peer + '.bmp')
        if source.stat().st_size != frame['bytes']:
            raise RuntimeError('native frame size differs: ' + str(source))
        shutil.copy2(source, target)
        images.append(target.name)
document = f'''# Neues Gebäude gemeinsam bauen und entfernen

[Normale Client-Platzierung](../artifacts/acceptance/{place_id}/summary.json) und [normaler Client-Löschbefehl](../artifacts/acceptance/{remove_id}/summary.json) sind auf beiden Instanzen bestätigt.

Lauf `{place['runtime_run_id']}`, DLL `d4d8c8ac09e13c0e4f8e5470562d95e6d4e9b3a5945c1db21ebd729c38da94c8`. Native ItemButton-/Cursor-Erfassung, ein gemeinsamer neuer Objektschlüssel, originale Purchase/Add/Undo je einmal und Host-ACK beider Spieler. Kosten400, Geld94.200 nach Bau und94.600 nach Entfernung. Beide Instanzen haben gleiche20 Buchungsgruppen. Der Löschbefehl betrifft exakt den neu angelegten Objektschlüssel.

Alle vier Abschlussbilder wurden betrachtet. Nach der Entfernung ist die zusätzliche Inn weg; Baupalette und Vorschau sind geschlossen. Der ältere zusätzliche weiße Move-Marker bleibt als historische Beobachtung erhalten; sein ursprünglicher Lebenszyklus ist dadurch nicht erklärt.

[Gemeinsamer Sourcekandidat und Regressionen](../tools/combined_scene_build/handoff.md):16 C++-Quellen mit strengen Compilerprüfungen,13 Kauf-Gatefälle und2 Installerguards. Weitere Gebäudefamilien, Szenenobjekte, laufende Wirtschaft und Events bleiben offen.

'''
for image in images:
    document += '![Gemeinsamer Bau- und Löschstand](media/' + image + ')\n\n'
(ROOT / '_drafts/dynamic_build_remove.md').write_text(document, encoding='utf-8')
section = '<section id="dynamic-native-cycle"><h2>Neues Gebäude gemeinsam bauen und löschen</h2><p>Normale Client-Aktionen auf beiden Instanzen bestätigt. Baukosten400, nach Entfernung wieder94.600. <a href="dynamic_build_remove.md">Native Belege und Bilder</a>.</p>'
for image in images[-2:]:
    section += '<img src="media/' + image + '" alt="Gemeinsamer Zustand nach Entfernung der neuen Inn">'
section += '</section>'
for name in ('_drafts/progress.html', 'tools/update_progress.py'):
    path = ROOT / name
    content = path.read_text(encoding='utf-8')
    if '<section id="dynamic-native-cycle">' not in content:
        content = content.replace('</main>', section + '</main>')
    content = content.replace('Gebäude-Kauf im selben Frame, Szenenobjekte und lesende Baufehler-Diagnose werden zusammengeführt. Sourcechecks bestanden; die gemeinsame Spielabnahme steht aus.', 'Gebäude-Kauf und Szenenobjekt-Backend integriert und gebaut. Normales Bauen und Löschen einer neuen Inn auf beiden Instanzen bestätigt; Szenenobjekt-Abnahme steht aus.')
    path.write_text(content, encoding='utf-8')
state = ROOT / '_drafts/implementation_state.md'
content = state.read_text(encoding='utf-8')
start = content.index('Aktueller Kandidat:', content.index('## PLACE-UI-KISS laufende Umsetzung'))
end = content.index('\n\n## Frühere Place-/Move-Beobachtungen', start)
content = content[:start] + f'''Aktueller Kandidat: [COMBINED-SCENE-BUILD-1](../tools/combined_scene_build/handoff.md),31 Zielpfade vollständig integriert. [Build bestanden](../artifacts/build_history/20261012T214257214793Z/build-1.log), [Freeze](../artifacts/placement_kiss/20261012T214431789649Z/freeze.json), DLL d4d8c8ac09e13c0e4f8e5470562d95e6d4e9b3a5945c1db21ebd729c38da94c8. Aktueller Lauf {place['runtime_run_id']} nach [Recovery](../artifacts/acceptance/be373468-d949-4538-a935-44076e308d52/summary.json) aus vollständigem Host-Save eec2013a. Native Titelbilder vor Load erfasst und Hostbild betrachtet; UI-Agent nutzt sie für echten Desktopstil.

[Neue normale Platzierung und dynamischer UI-Remove](dynamic_build_remove.md) tatsächlich bestanden. BauUUID {place['action_uuid']}, RemoveUUID {remove['action_uuid']}; neue Inn je einmal, native Purchase/Add/Undo, Host-ACK und echte Referenzfreigabe. Geld94.200 nach Bau,94.600 nach Entfernung; alle20 Buchungsgruppen gleich. Vier Abschlussbilder betrachtet; Vorschau und Baupalette nach Remove geschlossen. Frühere normale Moves bleiben übernommen. Der frühere weiße Move-Marker ist historisch erhalten, seine Ursache bleibt ungemessen.

Der Bau-Timeout df5d7308 am Punkt8200/8550 bleibt erhalten. Dieser neue Lauf verwendet den zuvor erfolgreichen Punkt8100/8550; deshalb erklärt der Erfolg nicht nachträglich die Ursache des Timeouts. Neue lesende Cursor-/Pending-Receipts und Sameframe-Purchase-Gate sind enthalten, kein Flag-/Input-Zwang eingeführt.

Nächste Aktion: weitere Gebäudefamilie mit normaler UI auf zwei Instanzen, echte Scenery-GUI und laufender Host-Zustand. Root besitzt Produktion/Build/Spiel-MCP. action_session ergänzt SceneryInfoWindow, tatsächliche Place-Erfassung und serialisierte LinkIDs; engine_host_delta liefert NativeClock-/Finanzapply im bestehenden Controller; lobby_ui ergänzt nativen Desktop/Weltwahl/Start. Keine neue Controllerplattform, keine unveränderten Spielaktionen erneut testen.

[Lobbyentwurf](lobby_ui.html):23 Zustände und [zwölf Callbackchecks](../tools/lobby_ui/checks.json). Nativer Desktop/Weltwahl/Start und Steamadapter noch offen. Source-Szenenobjekte sind integriert; native Create/Move/Remove bleiben zu prüfen.''' + content[end:]
state.write_text(content, encoding='utf-8')
print('info: native build/remove cycle published; full coop remains incomplete')
