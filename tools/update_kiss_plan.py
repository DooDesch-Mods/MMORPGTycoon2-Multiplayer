"""Publish the current coop priority without changing a game runtime."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
plan = '''# Spielbarer Koop mit YAGNI und KISS

Stand: 12.10.2026. Neues aktives Ziel: spielbarer Zweier-Koop mit dem einfachsten vorhandenen Aufbau. Gemeinsames Bauen zuerst; vollständiger Multiplayer bleibt beauftragt. Kleine lokale Welt zuerst, vier Instanzen, große Welt und Online-Abnahme zuletzt.

## Einfacher Aufbau

Eine Sitzung, ein Transportadapter und ein Host-geordneter Aktionsweg. Spieler bedienen die normale Oberfläche. Die Mod erfasst vollständige Bauwünsche, der Host führt sie aus, die Clients übernehmen bestätigte Ergebnisse. Ein kleiner Multiplayer-Zustand hält gemeinsame Objekt-IDs, Objektparameter und die aktuelle Revision. Lokale native Objektverweise werden getrennt aufgelöst.

Vorhandene native Spielroutinen übernehmen ihre eigenen Folgewirkungen. Neue allgemeine Engineplattformen, ein zweiter Controller und vorsorgliche Sonderfallinfrastruktur entfallen. Deduplizierung, aktuelle Objektzuordnung und sichtbare Fehler bleiben erforderlich, damit ein Klick keine doppelten Objekte oder Buchungen erzeugt.

## Umsetzungsphasen

| Phase | Umfang | Sichtbare Abnahme |
|---|---|---|
| 1 - Gemeinsam bauen | Normales Platzieren, Bewegen und Löschen an den bestehenden Aktionscontroller anschließen. Mit SmallInn beginnen, anschließend Gebäude und Dekoration erweitern. | Beide Spieler setzen und bearbeiten Objekte über ihre normale Oberfläche. Jede bestätigte Aktion erscheint genau einmal auf beiden Instanzen, mit gleichen Objektparametern und Baukosten. |
| 2 - Zusammen spielen | Gemeinsam bauen bei laufender Host-Simulation. Die für das Spiel erforderlichen Zeit-, Geld- und Weltzustände übertragen. Speichern, Laden und Beitritt an den Bauzustand anschließen. Weitere Aktionsfamilien nach der [Aktionsmatrix](action_matrix.md) ergänzen. | Zwei Spieler können dieselbe kleine Welt tatsächlich bauen und verwalten. Kamera und Oberfläche bleiben individuell, notwendige gemeinsame Werte stimmen. |
| 3 - Umfang und Robustheit schließen | Übrige Spielsysteme, Undo/Redo, Chat, Cursor, Reconnect und Fehlerfälle fertigstellen. Danach vier Instanzen, große Welt und Steam auf getrennten Rechnern prüfen. | Vollständiger vereinbarter Koopumfang besteht, einschließlich Persistenz und finaler Abnahmen. |

Die ersten isolierten Bauprüfungen laufen pausiert. Pause ist keine dauerhafte Gameplay-Grenze. Vollständige Scheduler- und Wirtschaftsanalysen halten eine einzelne nachweisbare Bauaktion nicht pauschal auf. Unvermeidbare Bedingungen des tatsächlich benutzten nativen Aufrufpfads werden geprüft.

## Aktuelle Umsetzung

Hauptagent: Produktion, Build, MCP und native Zweierabnahme. Engine-Subagent: engster Capture-/Replayanschluss an die originale Bauaktion. Sitzungs-Subagent: vorhandenen Remove-Controller minimal auf Place/Move erweitern. Zustands-Subagent: kleinste gemeinsame ID-/Objektzustandsschicht anhand der offenen Engine und der tatsächlichen Modanschlüsse, ohne zweiten Controller.

Erster Kandidat PLACE-UI-KISS: normale SmallInn-Platzierung vor dem Effekt erfassen und über eine vollständige native Bauaktion replizieren. Neue Risiken sind doppelte lokale Effekte, falsche Objektzuordnung und ein teilweise ausgeführter Commit. Kleinster Check: je ein echter Host- und Clientbau, Objekt-/Transform-/Kostenreadback, Bestätigung beider Peers und MCP-Spielbilder. Danach dieselben neu erzeugten Objekte bewegen und löschen.

[Diagnostische Platzierung und Persistenz](small_persistence.md), [normales Entfernen](ui_remove_two.md) und [gemeinsame Zeitbuttons](time_mouse_two.md) bleiben bestanden. Neue Tests betreffen den geänderten Capture-/Controller-/ID-Pfad; unveränderte Meilensteine werden nicht wiederholt.

Die [offene Enginequelle](../tools/engine_source/vectorstorm) hilft bei APIs und Lebenszyklen, enthält aber nicht automatisch die MMO-Spielklassen oder den exakten installierten Build. Ein Engine-Fork ersetzt daher nicht den Anschluss an die laufende Spielinstallation. [Simulationsanalyse](simulation_owner_census.md) und [Wirtschaftsanalyse](business_runtime.md) bleiben Hintergrund für erforderliche spätere Eingriffe.

Private Saves bleiben erhalten. Spielbedienung erfolgt ausschließlich über MCP. Keine Veröffentlichung, kein Commit und kein Push sind beauftragt.
'''
section = '''<section aria-labelledby="placement-priority"><h2 id="placement-priority">Aktives Ziel: spielbarer Zweier-Koop</h2>
<p><a href="placement_phases.md">YAGNI-/KISS-Plan vom 12.10.2026</a>: 1. gemeinsam platzieren, bewegen und löschen; 2. gemeinsam bei laufender Host-Simulation bauen und verwalten; 3. übrigen Umfang und Robustheit schließen, danach finale Vierer-/Großwelt-/Online-Abnahme.</p>
<p class="open">PLACE-UI-KISS wird umgesetzt. Ein Host-geordneter Ereignisweg und ein kleiner gemeinsamer Objektzustand nutzen die vorhandenen Spielroutinen. Normales gemeinsames Platzieren ist noch nicht nativ bestanden. Drei Subagenten bearbeiten Engineanschluss, Controller und Objektzustand; der Hauptagent integriert und prüft über MCP.</p></section>'''

(ROOT / '_drafts/placement_phases.md').write_text(plan, encoding='utf-8')
for name in ('_drafts/progress.html', 'tools/update_progress.py'):
    path = ROOT / name
    content = path.read_text(encoding='utf-8')
    start = content.index('<section aria-labelledby="placement-priority">')
    end = content.index('</section>', start) + len('</section>')
    path.write_text(content[:start] + section + content[end:], encoding='utf-8')
    print('info: updated ' + name)

path = ROOT / '_drafts/implementation_state.md'
content = path.read_text(encoding='utf-8')
start = content.index('Neue Priorität vom 12.10.2026:')
end = content.index('\n\n', start)
replacement = ('Neue aktive Priorität vom 12.10.2026: [YAGNI-/KISS-Plan](placement_phases.md). '
               'Spielbarer Zweier-Koop mit dem einfachsten vorhandenen Aufbau: ein Ereigniscontroller, '
               'kleiner gemeinsamer Objektzustand, normale native Platzierungs-/Bewegungs-/Löschroutinen. '
               'PLACE-UI-KISS wird implementiert. Drei Subagenten bearbeiten Engineanschluss, Controller '
               'und Objektzustand; Hauptagent allein Produktion/Build/MCP/native Abnahme. '
               'Bestandene Nachweise bleiben übernommen. Globale Engineanalysen werden nur für die '
               'tatsächlich erforderlichen Spielpfade genutzt. Keine fehlende Nutzerfreigabe. '
               'Die älteren nächsten Schritte darunter sind historische Aufzeichnungen.')
path.write_text(content[:start] + replacement + content[end:], encoding='utf-8')
print('info: updated _drafts/implementation_state.md')
