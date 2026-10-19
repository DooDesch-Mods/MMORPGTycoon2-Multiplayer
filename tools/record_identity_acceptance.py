"""Retain the current native identity evidence and update the existing handoff."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
acceptance = ROOT / 'artifacts/acceptance/2777927c-26f7-4688-8ccf-595b59802526/summary.json'
record = json.loads(acceptance.read_text(encoding='utf-8'))
assert record['proof']['host_native_capture'] and record['proof']['transfer_and_client_load']
assert not record['passed'] and record['step'] == 'client-final-readback'
checks = {entry['check']: entry['result'] for entry in record['checks']}
capture = checks['host-native-capture']
assert capture['snapshot_id'] == '7a8399f7-e80d-4dc4-b2f1-ec829f5f3861'
sdk = json.loads((ROOT / 'test/mcp/evidence/checks.json').read_text(encoding='utf-8'))
log = (ROOT / 'test/mcp/evidence/checks.log').read_text(encoding='utf-8')
assert sdk['passed'] and 'Ran 58 tests' in log
for relative, expected in sdk['source_sha256'].items():
    assert hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() == expected, relative
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
destination = ROOT / 'artifacts/mcp_checks' / stamp
destination.mkdir(parents=True, exist_ok=False)
shutil.copy2(ROOT / 'test/mcp/evidence/checks.log', destination / 'checks.log')
sdk['log'] = str(destination / 'checks.log')
(destination / 'result.json').write_text(json.dumps(sdk, indent=2) + '\n', encoding='utf-8')
sdk_link = f'../artifacts/mcp_checks/{stamp}/result.json'
for role, filename in [('host', 'identity-host_20261012T033924676520Z.json'), ('client', 'identity-client_20261012T033929219960Z.json')]:
    evidence = json.loads((ROOT / 'test/mcp/evidence' / filename).read_text(encoding='utf-8'))
    assert evidence['passed']
    response = next(c['result']['structuredContent'] for c in evidence['calls'] if c['tool'] == 'game_capture_frame')
    assert response['ok'] and response['run_id'] == '724f7be6-f30b-49ad-9646-f96d8899e69c'
    source = Path(response['result']['path']).resolve()
    source.relative_to((ROOT / '_MPTest').resolve())
    shutil.copy2(source, ROOT / '_drafts/media' / f'identity_{role}.bmp')

state_path = ROOT / '_drafts/implementation_state.md'
state = state_path.read_text(encoding='utf-8')
start = state.index('Owner: Hauptagent für Core')
end = state.index('[BUILD-CAPTURE-1 Spielprüfung]', start)
current = f'''Owner: Hauptagent für Core, Build, MCP, Laufzeit und Abnahme. Eigener Lauf `724f7be6-f30b-49ad-9646-f96d8899e69c`, DLL-SHA256 `4968c30d76c726681768b5b9b51bd1bf0d5a71797a9850afe67beabcf1fb73e8`. Release-Build, CTest3/3 und Proxy-Exporte bestanden; der erste Protokoll-Idle-Test scheiterte an seiner zu engen Ursachenannahme. [Gezielte Protokollkorrektur](../artifacts/build_jobs/network_identity_recovery_20261012/status.json) besteht ohne Produktionsänderung daraus.

Aktuelle [native Abnahme](../artifacts/acceptance/2777927c-26f7-4688-8ccf-595b59802526/summary.json) scheitert sichtbar an der Client-Netzwerkkarte. Datei `7a8399f7-e80d-4dc4-b2f1-ec829f5f3861`, {capture['bytes']} Bytes, SHA256 `{capture['sha256']}` ist vollständig erfasst, geprüft, übertragen und im Client geladen. Gebäudezuordnung: `before: ContainsObject missing positive link actual=0`. Client-Netzwerkarm: `native save still active actual=-1 required=0`. Native Original-Saveaufrufe150/150, keine Ausnahme. Dateiladen bestätigt keine Objektkarte und keinen vollständigen Koop.

Vorherige [Host-Load-Beobachtung](../artifacts/acceptance/267d976a-96d2-4b64-8638-46fadd47620b/summary.json) endete nach bestätigtem Load an einer kurzzeitig fehlenden MCP-Antwortdatei. Genau dieser bestehende ACK wurde übernommen, kein Load wiederholt. Enger WinError2-Publikationswait bewahrt Lock und UUID; andere Dateifehler bleiben Fehler. [SDK58/58]({sdk_link}). Native Host-Loadzeit125197ms, noch keine kausale Erklärung für den Unterschied zur vorherigen Messung30838ms.

Native Frames betrachtet: [Host](../test/mcp/evidence/identity-host_20261012T033924676520Z.json) und [Client](../test/mcp/evidence/identity-client_20261012T033929219960Z.json). Beide zeigen dieselbe Karte, 129178 Subscriber, Geld8967530, Kapazität5600 und Uhr23:22:56. Das ist ein pausierter Snapshot-Nachweis. Private Eingangsdatei blieb unverändert.

Nächster Kandidat: NET-ID-TITLE-ENTRY-1 erlaubt nur den nativ geprüften stabilen Titelmodus2 ohne Welt und Übergang mit sentinel-1; Host-Saveguard bleibt0. Source116Fixtures bestanden, noch nicht deployed. Gebäude-Inline-Pfad ohne explizite LinkID ist eine neue belegte Hypothese; kein pauschaler Guard-Skip. Owner game_mcp untersucht Savevertrag, engine_bridge Load-/Strukturidentität, local_launcher NETSESSION-Staging. Hauptagent integriert und führt allein echte Spielproben aus.

'''
state = state[:start] + current + state[end:]
state = state.replace('59 Layoutprüfungen bestehen; korrigierter Save-Hook ist noch nicht deployed.', '59 Layoutprüfungen bestehen; korrigierter Save-Hook ist in DLL4968 deployed. Der aktuelle Versuch passiert den Writer-Guard und scheitert danach am bisherigen LinkID-Vertrag.')
state = state.replace('Die nachträgliche frische Completion-Deadlineprüfung nach finaler SHA ist nur im Quellcode, noch nicht in diesem Build.', 'Die frische Completion-Deadlineprüfung nach finaler SHA ist in DLL4968 deployed.')
state_path.write_text(state, encoding='utf-8')
contract_path = ROOT / '_drafts/mcp_contract.md'
contract = contract_path.read_text(encoding='utf-8')
contract = contract.replace('mit 55 Tests: [Prüfquittung](../artifacts/mcp_checks/20261012T014312169436Z/result.json)', f'mit 58 Tests: [Prüfquittung]({sdk_link})')
contract += '\nAktueller Publikationsvertrag: Ein ausschließlich nach vorherigem Exists auftretender WinError2 beim Antwortlesen ist ein erwarteter begrenzter Publikationswait. Der bestehende Request-Lock bleibt erhalten; keine Mutation wird erneut gesendet. UUID, Run-ID und Schema werden weiterhin geprüft. Andere IO- und JSON-Fehler bleiben sichtbar. Die native Abnahme2777927c bestätigt Dateiladen, aber scheitert getrennt an Client-Netzwerkarm und Gebäudeidentität. [Native Quittung](../artifacts/acceptance/2777927c-26f7-4688-8ccf-595b59802526/summary.json).\n'
contract_path.write_text(contract, encoding='utf-8')
progress_path = ROOT / 'tools/update_progress.py'
progress = progress_path.read_text(encoding='utf-8')
progress = progress.replace('55 SDK-/Mailbox-Prüfungen bestanden', '58 SDK-/Mailbox-Prüfungen bestanden')
progress = progress.replace('../artifacts/mcp_checks/20261012T014312169436Z/result.json', sdk_link)
progress = progress.replace('Writer-Übergang korrigiert, Spielprobe offen', 'Writer bestanden, Gebäude-Inline-ID offen')
progress = progress.replace('90.190.744 Bytes, vollständiger Client-Load und 150 Originalrückkehrer', 'Vorheriger Save: 90.190.744 Bytes und 150 Originalrückkehrer')
anchor = '<tr><td>Snapshot-Objektbestätigung</td>'
row = '<tr><td>Große aktuelle Spielprobe</td><td class="open">Dateiladen bestanden, Client-Netzwerkkarte gesperrt</td><td><a href="../artifacts/acceptance/2777927c-26f7-4688-8ccf-595b59802526/summary.json">90.189.198 Bytes, SHA geprüft und nativ geladen</a>; Titel-sentinel-1 und Inline-Gebäude-ID bleiben getrennte Blocker</td></tr>\n'
assert anchor in progress
progress = progress.replace(anchor, row + anchor, 1)
progress_path.write_text(progress, encoding='utf-8')
print(f'info: native evidence retained, SDK58={destination}, full_coop_proven=false')
