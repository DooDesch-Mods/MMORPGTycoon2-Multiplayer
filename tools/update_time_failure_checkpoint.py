"""Publish the retained user failure and current candidate without game mutations."""
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[1]
STATE=ROOT/'_drafts/implementation_state.md'
text=STATE.read_text(encoding='utf-8')
start=text.index('## Aktueller gemeinsamer Remove- und UI-Kandidat')
end=text.index('## Aktueller MCP-Eingabekandidat',start)
text=text[:start]+'''## Aktueller Zeitbutton-Fehler und Kandidat

Lauf01022b0e-564b-4718-bbc6-787c0fd867b9 mit DLLa79e3b339da7cfa3d6a8b1ec517d8c2a33c8f5e04dafed63e44f202e518d81bb: Nutzer hat beide kleinen Welten bearbeitet. Client-Klick auf den schnellen Zeitbutton beendete die Sitzung. Beide Prozesse blieben aktiv. Der Activate-Hook wurde erreicht, erkannte das separate HUD-Item aber nicht: attempted1, matched0, suppressed0. Der Originalaufruf setzte die Client-Uhr lokal auf9. Zeit-/Kalenderguards scheiterten, Channel10 beendete die Sitzung. Die letzte generische Ursache verdeckte den ursprünglichen Clockfehler; passive OriginalOnce meldete fälschlich Recovery.

[Unveränderter Fehlercheckpoint](../artifacts/user_time_failure/20261012T145623477425Z/checkpoint.json) enthält beide lokalen Savekopien mit überprüfter Größe/SHA256, Logs, Status, Manifeste und Nutzerbild. Beide Instanzen sind inzwischen per MCP pausiert. Host-Save232185Bytes und Client-Save224519Bytes sind erhalten. Host-Snapshotidentitäten sind für die neu gebauten Objekte nicht vollständig bestätigt; vollständige Dateipersistenz ist belegt, sichere laufende Synchronisation damit nicht. Die Nutzerwelten bleiben bis zum konkreten Fix erhalten.

Root-Kandidat HUD-MOUSE-TIME-1: Der vorhandene native Reader prüft tatsächliche HUD-/Slot-/Button-/Item-/Typebesitzer, exakte WeakzieleItem+20/Type+38 und zwei vollständige Pointerzählungen. Mouse-Rücksprung58095C wird mit dem vorhandenen Zeitcontroller verbunden. Client-Mouse-Zeitänderungen unterdrücken den nativen lokalen Effekt auch bei Guardfehlern; nur vollständige gültige HUDdaten dürfen einen Host-Auftrag erzeugen. Keyboard-Inventorypfad bleibt erhalten. Kalendertransport erhält ursprüngliche und Cleanupursachen; passive Anzeige meldet keine Recovery. Ein Subagent prüft Reader/Adapter mit tatsächlichem Controller, ein zweiter die Kalenderregressionen. Der Engine-Agent liefert einen vollständig geguardeten nativen ItemButton.Activate-MCP-Helfer; echte Host-ACK und Client-Clock0 müssen Root anschließend belegen. Noch kein neuer Build oder Ingame-Fix bestätigt.

[TypedRemove2](typed_remove_two.md) bleibt bestanden: ein Client-Auftrag, je eine native Ausführung und Freigabe, Host-ACK-Maske3, Revision1, Geld95000 und20 gleiche Buchungsgruppen. NATIVE-UI-REMOVE-1 ist gebaut und im aktuellen Lauf geladen; 80 Zielguard-,65 Bridge-,59 Consumer- und4 SDKprüfungen sind bestanden. Normale BuildingInfoWindow-Entfernung ist noch nicht ausgeführt. Der Zeitbuttonfehler hat Vorrang; die Nutzerwelten sind unabhängig verändert und eignen sich nicht als gleiche Remove-Ausgangswelt.

OwnerRoot: Produktion, Build, MCP und native Abnahme; Kinder nur eigene Staging-/Testpfade. Kein Computer Use. Zuerst echter HUD-Klick und Host-Quittung in der kleinen Zweierwelt, danach normale UI-Entfernung. Business-/Simulationsgrenzen, kompletter Baucommit/Undo, weitere Aktionen, Steam/Online bleiben offen. Vier Instanzen und große Welt sind finale Abnahmen.

## Frühere MCP-Eingabekandidaten
'''+text[end+len('## Aktueller MCP-Eingabekandidat'):]
STATE.write_text(text,encoding='utf-8')
script=ROOT/'tools/update_progress.py'
text=script.read_text(encoding='utf-8')
old='Client-Anforderungen für 1x, 3x und 9x sowie Host-Pause sind bestätigt. Bedienleisten und sichtbarer Host-Zeittext stimmen überein; die eigene Client-Uhr bleibt angehalten.'
new='Zeitsteuerung über native MCP-Tasteneingaben ist bestätigt. Ein echter Client-Klick auf den schnellen HUD-Zeitbutton setzte die eigene Uhr auf9 und beendete die Sitzung. Beide bearbeiteten Welten sind lokal mit geprüften Prüfsummen gesichert; beide Instanzen sind pausiert. Der Mouse-Fix ist in Arbeit und noch nicht im Spiel bestätigt.'
if old in text:text=text.replace(old,new,1)
marker='<table><thead>'
note='''<p class="open"><a href="implementation_state.md">Aktueller Blocker</a>: HUD- und Inventory-Zeititems sind separate Objekte. Reader-/Controllerregressionen sichern den neuen Mousepfad; <a href="time_failure_tests.md">Kalenderregressionen</a> prüfen ursprüngliche Fehlerursache und falsche Recovery. <a href="../artifacts/user_time_failure/20261012T145623477425Z/checkpoint.json">Unveränderter Nutzerfehler und beide Savekopien</a>.</p>
<details><summary>Nutzerbild der beendeten Sitzung</summary><img src="media/client_session_ended.png" alt="Client: Sitzung beendet nach Zeitklick"></details>
'''
if 'Nutzerbild der beendeten Sitzung' not in text:text=text.replace(marker,note+marker,1)
script.write_text(text,encoding='utf-8')
shutil.copy2(ROOT/'artifacts/user_time_failure/20261012T145623477425Z/client_session_ended.png',ROOT/'_drafts/media/client_session_ended.png')
print('info: current user failure and candidate checkpoint published; game unchanged')
