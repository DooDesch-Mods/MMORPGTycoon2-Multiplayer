"""Publish the current native failure and next causal fix."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
checkpoint = '''## PLACE-UI-KISS laufende Umsetzung

Aktives Ziel: spielbarer Zweier-Koop mit einem Host-geordneten Ereignisweg und kleiner gemeinsamer Objektzustandsschicht. Die offene Engine wird für konkrete Anschlüsse genutzt; kein Engine-Fork oder allgemeine Schedulerplattform ist Voraussetzung für einen einzelnen Bauauftrag.

Root hat Controller, gemeinsamen State-/ID-Adapter, normale Bauhooks, Original-Build-Replay, dynamische Remove-Zuordnung und SDK integriert. Build [173247769251](../artifacts/build_history/20261012T173247769251Z/build-1.log) besteht; DLL bcdd4431f73fafe792377ccec70641adc59a2ad1c04cac064bea7ec8a639b6ef. NativeCopyhashes entsprechen dem Engine-Staging. [Rootfreeze](../artifacts/placement_kiss/20261012T173739744034Z/freeze.json). Controller78, State8/Binding2/Registry8, Engine56 und fünf SDKmethoden sind separat belegt. Keine daraus abgeleitete Spielbereitschaft.

Eigener Lauf03d0b1d2-02c8-4699-ad34-c51f961f80a4. Beide ursprünglichen Testwelten wurden vor Neustart lokal erhalten: [Setup c1ac73bc](../artifacts/acceptance/c1ac73bc-e2c3-44ff-8157-c29c27e8264a/summary.json). Controller/Build-/Removehooks auf beiden Instanzen bestätigt. Erste Positionsprobe10920/6800 war ungekauft, ohne Bauauftrag; bestätigte Installation wurde weiterverwendet.

Der erste einzige Bauauftrag am tatsächlich eigenen Punkt7990/8550 endet unbestätigt: [Versuch d1169a9c](../artifacts/acceptance/d1169a9c-a10c-47ea-8dda-8aa93dbbb947/summary.json). Der Host-Spielthread wartet in einem nativen Assert; Client-ACK-Timeout ist sichtbar. [Tatsächliche Modul-/Crashoffets](../artifacts/acceptance/d1169a9c-a10c-47ea-8dda-8aa93dbbb947/crash_offsets.json) zeigen MT2.exe+E438CA im Identifier-Arrayzugriff des privaten Konstruktionspfads. Keine Aktion wiederholt. [Retained Fehlercheckpoint](../artifacts/placement_failure/20261012T175444496248Z/checkpoint.json).

Verworfene Hypothese: Build.C1 initialisiere Identifier[0] im gefilterten Array+1A0. Tatsächlicher C1 befüllt unfiltered+188; das gefilterte Array bleibt leer bis _ApplyArtFilters592450. Nächster gezielter Fix: Variant im nativen unfiltered Identifier setzen, originalen Filter einmal ausführen, gefilterten Count und Werte prüfen, erst dann Blueprint/OriginalUpdate. Native ABI-/Boundsbedingungen bleiben erforderlich; kein weiterer globaler Audit. Engine-Subagent liefert additive Initialisierung; Root integriert, baut und prüft erneut nach dieser neuen kausalen Änderung. Der vorhandene Auftrag wird nicht replayt.

Parallel liegen der normale Item-/Button-/Cursor-MCP-Anschluss und MOVE-KISS mit183 gezielten Sourcechecks vor. Root besitzt Produktion, Spiel und Abnahme. Private Saves bleiben erhalten, kein Computer Use, kein Commit/Push/Release. Vorherige Zeit-/Remove-/Persistenzbelege bleiben bestanden. Es fehlt keine Freigabe des Nutzers.

'''
path = ROOT / '_drafts/implementation_state.md'
content = path.read_text(encoding='utf-8')
marker = '## Aktueller nativer Zeitbutton-Stand und UI-Remove'
if checkpoint not in content:
    path.write_text(content.replace(marker, checkpoint + marker, 1), encoding='utf-8')

html = '''<p class="open"><strong>Aktuelle native Bauprobe:</strong> Ein Client-Auftrag wurde einmal gesendet. Der private Bauwerkzeug-Aufruf löste einen Engine-Assert beim Zugriff auf eine noch leere Variantenliste aus. Die fehlende originale Filterinitialisierung wird ergänzt. <a href="implementation_state.md">Fehler und nächster Fix</a>; keine Bauaktion wurde wiederholt. Gemeinsam platzieren ist noch nicht bestanden.</p>'''
for name in ('_drafts/progress.html', 'tools/update_progress.py'):
    path = ROOT / name; content = path.read_text(encoding='utf-8')
    if html not in content:
        content = content.replace('</section>', html + '</section>', 1)
        path.write_text(content, encoding='utf-8')
    print('info: checkpoint updated ' + name)
