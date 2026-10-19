"""Publish the frozen time bridge and update the current evidence record."""
import hashlib
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
staging = root / 'tools/engine_analysis/calendar_authority/ui_time/tests/staging'
hashes = {
    'shared_time.cpp': '0cdec35072c657b9c57b33b322890acdd80b48c375eec7ef93e5d2aee823cb8a',
    'shared_time.h': 'b3b64308674e0e6171f60bb44a2b2b92f8ecc808b36ea1e0767266ab327f2a0a',
}
for name, expected in hashes.items():
    actual = hashlib.sha256((staging / name).read_bytes()).hexdigest()
    if actual != expected:
        raise RuntimeError(f'Frozen source hash mismatch: {name}, actual={actual}, required={expected}')
for name in hashes:
    shutil.copy2(staging / name, root / 'src/game' / name)
record = root / '_drafts/implementation_state.md'
text = record.read_text(encoding='utf-8')
old = 'Große Abnahme `2e1e7cf3-4e48-4013-8058-15ce30ed39db` läuft mit verpflichtendem Readback aller 625 Gebäude.'
new = ('Große Abnahme `2e1e7cf3-4e48-4013-8058-15ce30ed39db` ist beendet: Host-Load, frischer Save, vollständige SHA-Übertragung und Client-Load bestehen. '
       'Host-Reader bestätigt 625 Gebäude, Client-Reader 232, jeweils ohne Ablehnung. Die bisher feste Client-Erwartung 625 scheitert; '
       'der tatsächliche native Save-Umfang ist noch zu klären. Beide eigenen Welten sind pausiert.')
if old not in text:
    raise RuntimeError('Current acceptance checkpoint text changed; preserve newer record')
text = text.replace(old,new).replace('ROOT-DUNGEON-2 ist gebaut und deployed, reale Abnahme läuft.',
    'ROOT-DUNGEON-2 ist gebaut und deployed; Host und Client bestätigen alle tatsächlich gelesenen Type1-/Type2-Gebäude.')
text = text.replace('UI-TIME-2, BUILD-ID-1 und NET-ID-1 bleiben außerhalb des Produktionsglob im Staging, bis Source und Fixtures abgeschlossen sind.',
    'UI-TIME-2 und NET-ID-1 sind als eingefrorene Quellen veröffentlicht, aber noch nicht im neuen gemeinsamen Build. '
    'BUILD-ID-1 und der Root-Katalog bleiben im Staging. UI-TIME-2 wird zuerst ausschließlich lesend an MCP angebunden; '
    'keine Zeitbutton-Hooks oder nativen Stores sind aktiviert. Risiko: falsche Engine-Adressen; Check: Build-/Thread-/Welt-/Besitzguards, SDK-Vertrag und echte Client-Lesung.')
record.write_text(text,encoding='utf-8')
progress = root / 'tools/update_progress.py'
text = progress.read_text(encoding='utf-8')
text = text.replace('neue Subscriber-Anwendung im echten Spiel noch offen','einzelne Subscriber-Anwendung im echten Spiel bestätigt')
text = text.replace('625 Gebäude erfasst; 29 Dungeon-Readbacks scheitern am Shard-Guard','Host liest 625, Client 232 Gebäude ohne Readerfehler; Save-Umfang noch offen')
text = text.replace('Type1 unterstützt; Type2 bleibt offen','Type1 und Type2 nativ gelesen')
text = text.replace('94bc8a7c-c4e3-4cab-a942-5f307738f583/summary.json','2e1e7cf3-4e48-4013-8058-15ce30ed39db/summary.json')
text = text.replace('Neuer Host-/Client-Load bestätigt; vollständige Gebäudemap abgewiesen','Host 625 und Client 232 Readbacks ohne Ablehnung')
text = text.replace('Host 25 verbleibende Type2-Readbacks','gemeinsame Capture-Identität bleibt offen')
progress.write_text(text,encoding='utf-8')
print('Frozen time bridge published; current acceptance checkpoint updated; native build not run.')
