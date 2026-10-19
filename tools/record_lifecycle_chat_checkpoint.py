"""Keep one current checkpoint and preserve its prior evidence block."""
from datetime import datetime, timezone
from pathlib import Path

root=Path(__file__).resolve().parents[1]
path=root/'_drafts/implementation_state.md'
source=path.read_text(encoding='utf-8')
start='<!-- working-checkpoint:start -->'
end='<!-- working-checkpoint:end -->'
if source.count(start)!=1 or source.count(end)!=1:
    raise RuntimeError('working checkpoint markers missing or ambiguous')
left,tail=source.split(start,1)
previous,right=tail.split(end,1)
archive=root/'_drafts/checkpoint_history_20261013.md'
with archive.open('a',encoding='utf-8') as stream:
    stream.write('\n\n## Checkpoint vor '+datetime.now(timezone.utc).isoformat()+'\n'+previous)
current='''
Ziel: vollständiger spielbarer Zweier-Koop, kleine Welt zuerst. Root besitzt Produktion, Build und eigene MCP-Laufzeiten. Kein Computer Use, keine privaten Saves verändern. Bestandene Aktionen bleiben geschlossen; volle Funktionsmatrix, vier Instanzen, Großwelt und physischer Steam-Test bleiben offen.

Kandidat: [Build154014211903](../artifacts/build_history/20261013T154014211903Z/build-1.log), [Freeze154457698039](../artifacts/placement_kiss/20261013T154457698039Z/freeze.json). Neue Desktop-App/Icon integriert, Channel9-Kind-Guard und Inline-Root-Loader integriert. Eigener Lauf60581161-a4d5-44fa-a8c4-fb40e8c492e6: Host62552/start134363799142290475, Client116028/start134363799144245463, Relay106216, Hostbridge58932, Clientbridge42504. Voriger Laufe8ab nach [beiden vollständigen Weltsavesb0c19cae](../artifacts/acceptance/b0c19cae-5009-443c-ad43-9dd868a969e6/summary.json) beendet/archiviert, Daten erhalten.

[Lobby-Laden mit positiven/Inline-Gebäudenccdfa](../artifacts/acceptance/ccdfa7a5-cda5-46ca-9491-a10d984eeca5/summary.json) nativ bestanden. Map+358 ist m_ownedRegions, eine Teilmenge der121 gespeicherten Regionen. [Channel9-Bau0733](../artifacts/acceptance/0733eaf2-c493-4a31-9124-941f8efd998f/summary.json) nach String-Type-Guard bestanden: neue SmallInn8020/8550, Kosten400, voller20er-Ledger, beide UID-/Pose-/ACK-Belege. Aktueller Bauweg wieder bestätigt. [History85939c5c](../artifacts/acceptance/85939c5c-5920-4d90-9f4a-51356bf8033b/summary.json) vor Original-Undo abgewiesen: UID7328 unverändert, native Y-Höhe70.577896 auf70.838318 gesetzt; alte BuildOperation.state war veraltet. Beide Controller terminalRejectedNoEffect2, pendingfalse/lockedfalse, failed1 in Read2f0e0eed. Keine Undo-Wiederholung. tools/building_history_followup/history.patch eingefroren, noch nicht integriert; nutzt bestehenden guarded refresh_current und originalen inversen DoMakeFunctionalBuilding-Graph für Redo. 50 betroffene Assertionen/TU-Compile bestanden. History-Runnertest korrigiert gemeinsame Frame-Snapshot-ID statt lokalem Host-Lade-UUID-Vergleich; neue Regression bestanden.

[Desktop-Readbackf11c](../artifacts/acceptance/f11c596f-a0d8-44a0-af92-3e1a8e38fa6e/summary.json) auf beiden bestätigt: Title2, entry_visibletrue, ownernative_game_desktop, keine Modearray-/Legacywindow-Registrierung. Originales Hostbild frame-aaacea26-5e76-479f-9992-82d07d67818c.bmp betrachtet: generiertes blau getöntes Peer-Icon/Spielschrift korrekt auf Desktop. [Desktop-Integration](../artifacts/desktop_app_integration/receipt.json), natives ClickView im Desktop-Content; assets/ui/desktop_multiplayer/multiplayer.png +generation.json mit fünf Originalreferenzen. Aktueller Setup-/Desktop-Klicklaufb17f9065 läuft; nach Rücklese Game3-Sichtbarkeitfalse/keineLegacywindow, Bild und Rückkehr prüfen. Kein Computer Use. Vier neue UI-Dateien plus PNG/Material im26-Datei-Laufzeitplan.

[Erster echter Client-C1-Import3d6ec046](../artifacts/acceptance/3d6ec046-71eb-4da2-ab14-4b3c3504205e/summary.json) bestanden: tatsächlicher Besucher2 auf Client, Count2 auf3/NextID3, Original ArrayLoad, eine Context-D1/Resolve und NotifyLogin. Lokale Toon-UID/Registry und Region bestätigt, sieben Needs bitgleich, vorhandene Kohorte und20 Finanzlinien unverändert. Record-/Context-/SaveScope-Cleanup bestätigt, Unknownfalse. [Capture1c08b74b](../artifacts/acceptance/1c08b74b-5309-4273-aec9-8c395d053d7e/summary.json), SourceRecordSHA5c528869de2496b6e87ae1ad29662cd8a0bf54a0337330f0b9524e4d0649dfd5. Einzelprobe geschlossen; automatische neue Besucher noch offen.

[Chat-UI59b43cc3](../artifacts/acceptance/59b43cc3-d4f6-4f70-9f0d-545cd9125998/summary.json) bestanden auf beiden: F9, echter ImGui-Fokus, Unicode-Text, Enter mit Empfänger-ACK/genau einem Eintrag, Escape, Mausmodus-Rückgabe und tatsächliche SDL-Tastaturweiterleitung. Beide originalen Bilder betrachtet und in progress.html erhalten. Der vorige Chat-Transport2c4fbde6 und elf Offline-E2E-Tests bleiben übernommen. SDL-Text wird bis zum bestätigten ImGui-Kopieren gehalten. [Integration](../artifacts/chat_integration/ui_receipt.json).

Geschlossen übernommen: Gebäude Place/Move/Remove, gemeinsame Zeit, einzelne Scene-/Terrain-/Region-Aktionen, erster Tageswechsel, lokaler Menü-Lifecycle, Roads, StartingPoint/Respawn/Uplink/Cable, Netz4b95bdfc und Host-InitialRelease00c8343d. Details und ursprüngliche Fehler im Checkpointarchiv behalten.

Nächste Schritte: Desktop-Abnahme abschließen; History-Patch exakt gegen aktuellen typed_action_runtime.cpp-Base6d502676 integrieren, bauen und eine frische Inn für Client-Undo/Host-Redo testen; danach tools/e2e/subscriber_additions.py zwei echte Zugänge samt C1/Frame/Host-ACK. Gezielte Offline-Checks durch Root: History3 plus neue Snapshot-Regression1, Besucher6, Region-Name/Kauf5 und Type3; geschlossen halten. Region-Stage enthält ClearCounts für Building/Scenery/Path/Network, UID-Lookup-Null und Registry/Table-Retirement. Native Region-Abnahme offen, weitere Manager ohne Count-Proof und Retail-LiftRegion-Toolbar nicht belegt. Altes Region-handoff.json veraltet: Auto-Review verweigerte Aktualisierung des alten Freeze/Rebase. Root erstellt später NEUEN Kandidaten aus tatsächlichen Stage-Hashes, erhält aktuelle snapshot_building_key/Core/Server-Deltas und ändert keine alten Frozen-Dateien.
'''
current='\n'+(root/'_drafts/working_checkpoint.txt').read_text(encoding='utf-8')+'\n'
path.write_text(left+start+current+end+right,encoding='utf-8')
print(path)
