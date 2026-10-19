"""Refresh the current input candidate record from retained build and native evidence."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
candidate = 'TIME-ACCEPTANCE-1-KEY-READINESS'
build = ROOT / 'artifacts/build_jobs/time_input_readiness_20261012_retry/status.json'
status = json.loads(build.read_text(encoding='utf-8'))
if status.get('state') != 'confirmed-execution' or status.get('exit_code') != 0:
    raise RuntimeError('input build checks are not confirmed')
dll = hashlib.sha256((ROOT / 'build/bin/Release/MT2_Multiplayer.dll').read_bytes()).hexdigest()
acceptance = ROOT / 'tools/engine_analysis/calendar_authority/time_acceptance/runs/73c766dc-83bd-4e85-ae87-8de500170859/summary.json'
native = json.loads(acceptance.read_text(encoding='utf-8'))
if dll != '1597f0c1daca7bdffeb718d9a4764aa1cd6c4e65195c46b560fbc2285f838539' or native.get('passed') is not True:
    raise RuntimeError('current build hash or native time acceptance differs')
text = f'''# Native MCP-Zeiteingabe

Kandidat `{candidate}`, DLL `{dll}`. [Buildbeleg](../artifacts/build_jobs/time_input_readiness_20261012_retry/status.json): Release, CTest3/3, Proxy-Exporte, 14 Launcher- und 42 Acceptance-Prüfungen bestanden. Keine Compilerwarnung im neuen Build. [SDKbeleg](../test/mcp/evidence/checks.json): 65 Prüfungen, einschließlich sieben neuer Zeitfälle. [Kopierbeleg](../artifacts/time_input_integration/readiness/copy_receipt.json): jede Taste wird nach vollständiger Bindingprüfung einzeln freigegeben.

Aktuelle [native Tastenabnahme](../tools/engine_analysis/calendar_authority/time_acceptance/runs/73c766dc-83bd-4e85-ae87-8de500170859/frame_checkpoint.json): neun bestätigte Inputs, jeweils genau ein nativer Capture und passende Hostquittung. Host und Client haben F1/F2/F3/F4 für Skalen0/1/3/9 ausgelöst; abschließendes Host-F1 pausiert beide. Native Client-Uhrsetter:0. Der originale GameUpdate läuft auf beiden weiter.

Der MCP reserviert einen unveränderlichen Token vor SDL-KeyDown/KeyUp. Ein Erfolg erfordert native Press-/Releaseflags, einen tatsächlichen Zeit-Leaf aus der Tastaturroute und die passende Hostquittung. Partial/Unknown blockieren weitere Eingaben. Beide betrachteten Abschlussbilder zeigen die kleine Welt ohne Tutorialdialog,100000 Geld,keine Subscriber und einen pausierten gelben Button. Hostkalender01:10:32 und Clientkalender00:00:28 unterscheiden sich. Laufende Buttondarstellung, Kalenderreplikation, Wirtschaft, vollständige Objektaktionen und Online-Koop bleiben offen.

Die unabhängige Delta-Review von NATIVE-OPEN-1 schließt Hashbuffer-Lifetime, vier Entryguards und immutable Root-/Hookbindung. Neuer Stagingfreeze: Header `15fb6dc0ed5ee7c6d0728ab629a13924c1816ea3112c3b06a826181483984fcb`, CPP `9e81f8960c860434d74e6be5a04b341e5dae5885e1996410fe031dfbc506450d`. [Vertrag](../tools/engine_analysis/object_identity/build_capture/native_open_contract.md) und [60 ausgeführte Guard-Beobachtungen plus eine Reparse-Skip](../tools/engine_analysis/object_identity/build_capture/evidence/native_open_checks.json). Kein NativeOpen-Hook wurde installiert. Die Own-Runtime muss parallele PhysFS-Mount- und Pfadmutationen ausschließen; der echte PhysFS-Handle ist noch kein Beleg.

Die [erste native Inputprobe](../tools/engine_analysis/calendar_authority/time_acceptance/runs/1172cc20-16f8-4e41-8a86-77fabb21ac1b/summary.json) scheiterte read-only an `time_input_binding_collision axis=21`, vor jeder Taste. Native Readbacks bestätigen Space an Axis16(A Button) und Axis21(Pause). F1-F4 sind einzeln sicher freigegeben; Space bleibt vor Eingabe gesperrt. Dynamische UI-Hotkeys verhindern bisher einen sicheren Ausnahmeguard. Erster Readiness-Installer scheiterte vor Dateikopie an CRLF/LF-Hashvergleich; normalisierte Formatprüfung behob die Ursache. Ein vorheriger Buildaufruf hatte einen fehlenden Jobpfad und führte keinen Compiler aus.

Aktuelle Nutzersteuerung: alle Funktionsprüfungen in der [frisch erzeugten kleinen eigenen Welt](../artifacts/acceptance/a5847503-7930-4bac-ba3d-2054d385f03b/frame_checkpoint.json),211573Bytes,UUID `f5acffea-9a64-49cd-8d06-1a5aa4ad6f4c`. Das Profilflag beseitigte den gespeicherten Tutorialdialog nicht; erst native Neuanlage berücksichtigte es. [Setupbefund](small_world_setup.md). Die große Welt bleibt finale Belastungsabnahme. Nächster Schritt: laufenden Button betrachten, danach NativeOpen-Ursprungsschutz integrieren und Zustandsreplikation prüfen. Pausierte Vierer-Abnahme und500-MiB-Transportbeleg bleiben historische Nachweise; sie belegen keine laufende Simulation oder einen echten500-MB-Engine-Load.
'''
(ROOT / '_drafts/time_input.md').write_text(text, encoding='utf-8')
state_path = ROOT / '_drafts/implementation_state.md'
state = state_path.read_text(encoding='utf-8')
marker = '## Aktueller MCP-Eingabekandidat\n'
section = '## Vorherige native Zeit-Runtime-Abnahme\n'
state = state.replace('## Aktuelle native Zeit-Runtime-Abnahme\n', section, 1)
addition = marker + f'\n[Native MCP-Zeiteingabe](time_input.md): DLL `{dll}`, Buildprüfungen und65SDK-Tests bestanden. Kleine Welt f5acffea ohne Tutorialdialog; neun echte F1-F4-Inputs mit einmaligem Capture/Hostquittung bestätigt. Beide Abschlussbilder betrachtet,pausiert. Kalender weicht ab: Host01:10:32/Client00:00:28. Space und laufende HUD-Darstellung offen. Große Welt finale Belastungsabnahme. Root besitzt Core,Hooks,Build und alle MCP-Gameaktionen. Kinder liefern getrenntes Staging. NativeOpen-Delta-Review geschlossen,Integration offen. Aktive Sitzung ff2f89f5-7db0-412b-a1f6-594bdb758c0c.\n\n'
if marker in state:
    start = state.index(marker)
    end = state.index(section, start)
    state = state[:start] + addition + state[end:]
else:
    state = state.replace(section, addition + section, 1)
state_path.write_text(state, encoding='utf-8')
print(f'info: current input candidate recorded; native={native["state"]}; nine F1-F4 inputs confirmed; calendar open')
