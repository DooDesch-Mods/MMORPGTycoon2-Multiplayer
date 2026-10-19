"""Publish retained native results and the current unresolved cash scope."""
import json
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
draft=ROOT/'_drafts'
move=json.loads((ROOT/'artifacts/acceptance/dc264a81-dde2-447e-a159-920a269b47b9/summary.json').read_text())
for row in move['checks']:
    if row['check']=='native-frame-artifact':
        source=Path(row['result']['path'])
        peer='host' if 'host' in source.parts else 'client-1'
        shutil.copy2(source,draft/'media'/('inn_move_'+peer+'.bmp'))
checkpoint='''## Aktueller Stand - Host-Zustand und Rückerstattung

Ziel und Autorisierung: voller spielbarer Zweier-Koop, kleine Testwelt zuerst. Root besitzt Produktion, Build und Spiel-MCP. Keine Computer Use, privaten Saveänderungen, Commits oder Veröffentlichung. Bei konkreten Blockern hilft Astra mit High Effort; ungelöste Probleme legt der Nutzer morgen früh selbst vor.

Aktuelle Instanzen: Lauf 4dfafcb9-ef44-4901-ac89-4055c1f357c6, DLL 644e0bc4cffa402ae5a5162bd3c5b85d5030d73dd5b8ac32fc2c765d4fed9303. [Gesicherte kleine Welt erfolgreich geladen](../artifacts/acceptance/f53b296d-acb1-4f26-ac86-11ec7418887a/summary.json). Getrennte Daten bleiben erhalten.

Neu übernommen: größeres Inn/base normal vom Client platziert, danach normal vom Client und Host verschoben. [Client-Place](../artifacts/acceptance/3c8e3182-2552-40e5-b185-3d5d05bc6465/summary.json), [Client-Move bestätigt vor späterem Host-Ground-Abbruch](../artifacts/acceptance/acd489e7-d0f7-495a-b5f1-f232ef78f3e9/summary.json), [Host-Move vollständig bestätigt](../artifacts/acceptance/dc264a81-dde2-447e-a159-920a269b47b9/summary.json). Beide Abschlussbilder betrachtet. Geld 93.600 auf beiden Instanzen. Diese Aktionen werden nicht wiederholt.

[Nativer Refundgetter](../artifacts/acceptance/2db8fe64-0057-419d-a4e0-a4e7e9cfc1eb/summary.json) misst 1.000 für das größere Gasthaus auf beiden Peers. Das ist ein tatsächlicher Getter-Nachweis und keine Annahme aus den Baukosten.

Offener Fehler: [einmaliger Client-Remove mit aktiver Host-Zustandsübernahme](../artifacts/acceptance/006bb002-b01c-4474-9ad9-2017a25c3546/summary.json). Beide Aktivierungen bestätigt; Host-Entfernung und Referenzfreigabe bestätigt. Client hat das Objekt ebenfalls entfernt, aber AddCash einmal unterdrückt. Danach weicht sein vollständiger Buchungszustand ab. Sitzung Unknown, Host-ACK nur 1 statt 3. Kein Replay und kein behaupteter Erfolg. [Client-Zustand erhalten](../test/mcp/evidence/inn-live-failure-client_20261012T224741929099Z.json). Astra High analysiert den CashPermit-Scope und liefert einen minimalen Fix mit betroffenen Regressionstests.

Vorheriger Hilfsskriptfehler f848f1a4: Werkzeugvorbereitung vor Controllerstart wurde ohne Ausführung abgelehnt. Reihenfolge korrigiert, anschließend echte Vorbereitung bestätigt. Der neue Buchungsfehler ist davon unabhängig.

Quellstand: SameDay und NativeRefund integriert. Scenery-GUI-Kandidat mit 17 exakt passenden Zielpfaden integriert und [vollständig gebaut](../artifacts/build_history/20261012T224612560343Z/build-1.log); noch nicht in die laufenden Instanzen ausgeliefert. DayRecord- und Rejoin-Kandidaten separat; native Lobby und Steam-ACK-Flow in Arbeit. Unveränderte frühere Spielbelege bleiben geschlossen.

Nächste Aktion: Astra-Fix prüfen und integrieren, danach aus dem vollständig gesicherten Host-Spielstand einen frischen betroffenen Test ausführen. Keine Mutation in der Unknown-Sitzung wiederholen. Parallel native Lobby und Scenery-/Rejoin-Anschluss fertigstellen. Vier Instanzen, große Welt und Online-Abnahme bleiben zuletzt.

'''
path=draft/'implementation_state.md'
text=path.read_text(encoding='utf-8')
start=text.index('## PLACE-UI-KISS laufende Umsetzung')
end=text.index('## Frühere Place-/Move-Beobachtungen',start)
path.write_text(text[:start]+checkpoint+text[end:],encoding='utf-8')
section='''<section id="live-state-current"><h2>Größeres Gebäude und laufender Host-Zustand</h2><p>Gasthaus auf beiden Instanzen platziert und vom Client sowie Host verschoben. Beide Geld 93.600. Nativer Rückerstattungsleser bestätigt 1.000 auf beiden Peers.</p><p class="open">Neue Host-Zustandsübernahme: Beim Entfernen wird die Client-Rückerstattung unterdrückt. Sitzung meldet Unknown; kein erneuter Löschversuch. Astra mit High Effort bearbeitet die Ursache. <a href="../artifacts/acceptance/006bb002-b01c-4474-9ad9-2017a25c3546/summary.json">Native Fehlerquittung</a>. <a href="implementation_state.md">Aktueller Arbeitsstand</a>.</p><img src="media/inn_move_host.bmp" alt="Größeres Gasthaus nach bestätigtem Host-Move"><img src="media/inn_move_client-1.bmp" alt="Dasselbe Gasthaus beim Client"></section>'''
for path in (draft/'progress.html',ROOT/'tools/update_progress.py'):
    content=path.read_text(encoding='utf-8')
    if 'id="live-state-current"' in content:raise RuntimeError('checkpoint already published: '+str(path))
    content=content.replace('</main></html>',section+'</main></html>')
    path.write_text(content,encoding='utf-8')
print('info: native milestone and original cash failure published; no game mutation')
