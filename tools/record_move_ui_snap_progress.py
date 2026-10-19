"""Update the current checkpoint while retaining earlier observations."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / '_drafts/implementation_state.md'
text = path.read_text(encoding='utf-8')
start = text.index('## PLACE-UI-KISS laufende Umsetzung')
end = text.index('## Aktueller nativer Zeitbutton-Stand und UI-Remove', start)
previous = text[start:end].replace('## PLACE-UI-KISS laufende Umsetzung', '## Frühere Place-/Move-Beobachtungen', 1)
current = '''## PLACE-UI-KISS laufende Umsetzung

Root besitzt Produktion, Build, Spiel-MCP und Abnahme. Ziel bleibt vollständiger spielbarer Zweier-Koop; kleine Welt zuerst, vier Instanzen und große Welt zuletzt. Kein Computer Use, keine privaten Saveänderungen, kein Commit oder Push.

Übernommen: [normale Client-Platzierung](normal_build_two.md), [direkter Client-Move](client_move_two.md), [direkter Host-Move](../artifacts/acceptance/398c51de-dc69-4d6a-87fc-d947b0608138/summary.json), normale Zeitbuttons, Baseline-UI-Remove und Persistenz. Beide direkten Moves erhalten UID1086, Geld94.600 und20 gleiche Buchungsgruppen; echter inverser Undo und Host-ACK3. Diese Aktionen werden nicht wiederholt.

[Normale Move-UI03dd0738](../artifacts/acceptance/03dd0738-fdfc-45e1-bf43-02c4e9464c98/summary.json) scheiterte vor Press/Release: Diagnose verlangte rawPoint==GhostPosition. Native Placement snappt X/Z und Bodenhöhe; der frühere Receipt enthält keine gemessene Ghost-Pose. Kein Move-Intent entstand. Client-Diagnose bleibt Unknown, kein Replay und kein behaupteter Client-Save. [Vollständiger Host-Save](../artifacts/acceptance/e0dd1f24-5aca-418d-b20f-29ddf1ba6c93/summary.json) bleibt erhalten.

Aktueller Kandidat: Gebäude-Lifecycle und Snapdelta integriert, [Build bestanden](../artifacts/build_history/20261012T202843873727Z/build-1.log), [Root-Freeze](../artifacts/placement_kiss/20261012T203001337888Z/freeze.json). Snapdelta übernimmt die tatsächlich gemessene vollständige Ghost-Pose nach Release; spätere Capture-Prüfungen bleiben exakt. [33 CPP- und zwei SDK-Checks](../tools/move_ui_snap_delta/runs/b31d983c-48b8-428a-ab90-192cc7eab7fe/summary.json), [34 Gebäude-Lifecycle-Checks](../tools/building_lifecycle_family/evidence/sealed_20261012T201959173224Z/summary.json). Neue Familien und normale Move-UI sind noch nicht nativ bestätigt.

Nächste Aktion: [Recovery3dc2c7fc](../artifacts/acceptance/3dc2c7fc-b61e-4716-b323-4b6b10dceee9/summary.json) behält aktuelle Readbacks und Logs und lädt den vollständigen Host-Spielstand in beide eigenen Instanzen. Danach gezielt normale Move-UI beider Spieler, neue Gebäudevarianten und dynamischer Remove. free_mcp_tests ergänzt den Fresh-Runner und Unit-Regressionen. engine_host_delta bearbeitet den schlanken laufenden Host-Zustand für Zeit/Wirtschaft im Staging. Scenery-Kandidat liegt separat vor. Kein zweiter Controller, kein globaler Census als Vorbedingung.

'''
path.write_text(text[:start] + current + previous + text[end:], encoding='utf-8')
for name in ('_drafts/progress.html', 'tools/update_progress.py'):
    item = ROOT / name
    value = item.read_text(encoding='utf-8')
    value = value.replace('Normale Move-Oberfläche und Host-Auftrag werden geprüft.', 'Direkter Host-Move ebenfalls bestätigt. Normale Move-UI: Rasterprüfung korrigiert und gebaut, native Abnahme läuft.')
    item.write_text(value, encoding='utf-8')
print('info: current Move checkpoint and progress updated; earlier observations retained')
