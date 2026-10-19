"""Record the current native save-hook candidate without changing runtime state."""
from pathlib import Path
import argparse
import json

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / '_drafts/implementation_state.md'
text = path.read_text(encoding='utf-8')
marker = '\nNATIVE-INVENTORY-1:'
start = text.index('## Aktueller Kandidat und Lauf')
end = text.index(marker, start)
current = '''## Aktueller Kandidat und Lauf

Owner: Hauptagent für Core, Build, MCP, Laufzeit und Abnahme. Aktuell laufend: f1cbb81a-6145-46a0-a877-a9b05b2048c2, DLL de91f55750c64d7806fd39edfe0cca2555b8566fc2f0481ef6df55c9e28dc112. Die genauen Prozessidentitäten stehen in `_MPTest/run.json` und `instance.json`; eigene Instanzen sind pausiert.

[NATIVE-INVENTORY-2](../artifacts/acceptance/2ccdce78-f0f3-4cf2-b9e7-1f336008a87d/summary.json) besteht: vollständige eigene Host-Datei, Client-Load, gleiche begrenzte Ledger-/Needs-Werte und vier native Zeititems. Der Root-Katalog liest vor und nach Host-Save sowie im Client jeweils596 Gebäude, sieben Regionen, keine Nullwerte, Ablehnungen oder Duplikate. Observer625/232 beweist deshalb keinen Verlust dieser Root-Gebäude. Portable Schlüssel und vollständige Zustandsreplikation bleiben offen.

Neuer Quellkandidat BUILD-CAPTURE-1 ist noch nicht deployed. Der native SaveArray-Wrapper führt Original genau einmal aus und beobachtet SavedLinkIDs. Die vollständige Datei erhält eine eigene SHA-/Längenquittung; fehlgeschlagene Gebäudezuordnung bleibt getrennt sichtbar. Native Savefrist900 Sekunden, Host-Ausfallmonitor weiterhin getrennt. Datei wird während verteilter Readbacks per Länge/Schreibzeit und abschließend per vollständiger SHA geprüft.

Review fand fehlende Bereinigung bei Ausnahme vor Pending-Status und eine Netzwerk-Sitzungs-ID statt echter Weltbindung. Core-Cleanup und Thread-/Readback-Status sind ergänzt; World-Witness ergänzt game_mcp vor Build. Nächster Check: isolierte Capture-Fixtures, gemeinsamer Build/CTest/Proxy, einmalige große native Save-/Load-Prüfung. Risiko: falsche native Receiver oder SavedLinkID-Annahmen; Originalsave bleibt unverändert, unbestätigte Zuordnung sperrt spätere Zustandswrites.

Agenten: game_mcp besitzt Capture-World-Witness und danach Subscriber-Lifecycle; local_launcher besitzt NET-STATE-1-Staging; engine_bridge besitzt TIME-SESSION-1-Staging. Keine eigenständigen Spielstarts oder MCP-Mutationen durch Agenten. NET-ID-1 ist kompiliert, aber noch nicht angebunden. Vollständige Koop-Abnahme bleibt offen.
'''
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--acceptance')
args = parser.parse_args()
if args.acceptance:
    import uuid
    acceptance_id = str(uuid.UUID(args.acceptance))
    receipt = json.loads((ROOT / 'artifacts/acceptance' / acceptance_id / 'summary.json').read_text(encoding='utf-8'))
    host = json.loads((ROOT / '_MPTest/host/instance.json').read_text(encoding='utf-8'))
    client = json.loads((ROOT / '_MPTest/client-1/instance.json').read_text(encoding='utf-8'))
    if host['run_id'] != client['run_id'] or receipt['runtime_run_id'] != host['run_id']:
        raise ValueError('Acceptance and both owned runtime manifests must name the same run')
    checks = {check['check']: check['result'] for check in receipt['checks']}
    captured = checks['host-native-capture']
    observed = checks['host-building-capture-receipt']
    current = f'''## Aktueller Kandidat und Lauf

Owner: Hauptagent für Core, Build, MCP, Laufzeit und Abnahme. Aktueller Lauf `{host['run_id']}`, Host PID{host['pid']}, Client PID{client['pid']}. Deployed DLL `{host['installed_binary_sha256']['MT2_Multiplayer.dll']}`. Exakte Prozessidentitäten stehen in `_MPTest/run.json` und `instance.json`. Beide Welten sind pausiert.

[BUILD-CAPTURE-1 Spielprüfung](../artifacts/acceptance/{acceptance_id}/summary.json): `{receipt['state']}`, bestanden={receipt['passed']}. Native Datei `{captured['snapshot_id']}`, {captured['bytes']} Bytes, SHA256 `{captured['sha256']}`. Vollständiger Transfer, nativer Client-Load, gleicher begrenzter Ledger-/Needs-Zustand und jeweils596 Root-Gebäude bestätigt. Native Originalaufrufe {observed['original_attempted']}/{observed['original_returned']}, Ausnahmen{observed['original_failed']}.

Gebäudezuordnung ausdrücklich fehlgeschlagen: `{observed['error']}`. Wrapper-Beobachtung {observed['wrapper_calls']}/{observed['wrapper_returns']}, staged{observed['staged_buildings']}. Datei-ACK bestätigt keine portable Objektkarte. game_mcp untersucht Writer-Kette und nativen Aufrufvertrag anhand dieser neuen Laufzeitbeobachtung, ohne Guards pauschal zu lockern. Gesamter Koop bleibt unbewiesen.

World-Witness-1 und Core-Cleanup sind deployed. Die nachträgliche frische Completion-Deadlineprüfung nach finaler SHA ist nur im Quellcode, noch nicht in diesem Build. Native Save-Task wird bei Beobachtungsfrist nicht abgebrochen; Ursache und `native_world.saves` bleiben sichtbar. Host-Ausfallmonitor bleibt unabhängig.

Agenten: game_mcp besitzt Writerdiagnose und Subscriber-Lifecycle; local_launcher besitzt NET-STATE-1-Lifetimefix und optionalen Snapshot-Metadatenvertrag; engine_bridge besitzt globale Root-Mitgliedschaft bei individuellen Dungeonkameras. NET-ID-1 ist kompiliert, noch nicht angebunden. HostTime und TimeSession sind eingefrorenes Staging. Nächster Schritt: präzise Writer-Ursache beweisen, danach korrigierten Save-Hook sowie NET-ID-1 integrieren und nativ prüfen. Kein Agent führt Spiel-/MCP-Mutationen aus.
'''
text = text[:start] + current + text[end:]
text = text.replace('Aktueller deployed Release-Build: DLL-SHA256 `003b', 'Früherer deployed Release-Build: DLL-SHA256 `003b')
text = text.replace('Nächster Kandidat korrigiert den ShardManager-Receiver, behält Offset und Mitgliedschaftsprüfungen.', 'Der nachfolgende NATIVE-INVENTORY-2-Kandidat korrigiert den ShardManager-Receiver und besteht die reale Root-Lesung.')
path.write_text(text, encoding='utf-8')
print(f'info: current candidate retained in {path}')
