"""Publish sealed source work separately from native game acceptance."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
checks = json.loads((ROOT / 'tools/lobby_ui/checks.json').read_text(encoding='utf-8'))
if checks.get('status') != 'passed' or checks.get('callbackScenarios') != 12:
    raise RuntimeError('lobby callback evidence is incomplete')
section = '''<section id="current-candidates"><h2>Nächste Integration</h2>
<p>Gebäude-Kauf im selben Frame, Szenenobjekte und lesende Baufehler-Diagnose werden zusammengeführt. Sourcechecks bestanden; die gemeinsame Spielabnahme steht aus.</p>
<p><a href="lobby_ui.html">Multiplayer-Menü: 23 bedienbare Entwurfszustände</a>. Zwölf C++-Callbackprüfungen bestanden. Nativer Desktop, Weltwahl und Sitzungsstart sind noch offen.</p>
<img src="media/lobby_ui/lobby.png" alt="Multiplayer-Lobbyentwurf mit Spielerplätzen und Weltwahl"></section>'''
for name in ('_drafts/progress.html', 'tools/update_progress.py'):
    path = ROOT / name
    content = path.read_text(encoding='utf-8')
    if '<section id="current-candidates">' not in content:
        content = content.replace('</main>', section + '</main>')
    path.write_text(content, encoding='utf-8')
state = ROOT / '_drafts/implementation_state.md'
content = state.read_text(encoding='utf-8')
start = content.index('Nächste Aktion:', content.index('## PLACE-UI-KISS laufende Umsetzung'))
end = content.index('\n\n## Frühere Place-/Move-Beobachtungen', start)
content = content[:start] + '''Nächste Aktion: Root integriert den versiegelten gemeinsamen Gebäude-Kauf-/Scenery-/Lifecycle-Kandidaten, baut und startet ausschließlich die zwei eigenen Laufzeiten aus dem vollständigen Host-Save eec2013a neu. Vor Laden entstehen native Titelbilder für den Menüvergleich. NormalMove und andere unveränderte Belege bleiben übernommen. Die gezielte neue Place-Prüfung muss genau ein Objekt und eine Abbuchung pro Peer zeigen, danach dynamischer Remove.

Staging: [Lobbyentwurf](lobby_ui.html) mit23 Zuständen und [zwölf Callbackchecks](../tools/lobby_ui/checks.json); nativer Desktop/Weltwahl/Start offen. Scenery-Create/Lifecycle Sourcechecks sind bestanden, native Abnahme offen. action_session ergänzt tatsächlichen SceneryInfoWindow-/LinkID-Anschluss; engine_host_delta ergänzt laufende Clock-/Finanzzustände im vorhandenen Controller. Root bleibt alleiniger Produktions-/Game-MCP-Owner.''' + content[end:]
state.write_text(content, encoding='utf-8')
print('info: source candidates published; native gates remain explicit')
