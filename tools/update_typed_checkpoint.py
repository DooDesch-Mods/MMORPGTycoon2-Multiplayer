"""Replace the stale current candidate record with the active integration state."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
path=ROOT/'_drafts/implementation_state.md'
text=path.read_text(encoding='utf-8')
start=text.index('Aktueller Umsetzungskandidat: REMOVE-RETIREMENT-DELTA-1')
end=text.index('\n[NativeRemove2 bestanden]',start)
replacement='''Aktueller Umsetzungskandidat: [NativeRetirement2](native_retirement_two.md) bestanden, Abnahme19ddc160-18ed-46ce-994c-27fbcd51ef65. Beide tatsächlichen Pools released1, held0, tombstones0, jeweils Release/D0/SizedDelete/CRT-Free1, SEH0. Native Quiescence und tatsächlicher Root-Registry-Widerruf bestätigen jede Freigabe. Die gefundenen fehlenden SEH-Nullprüfungen sind mit ursprünglichen Fehlerbelegen erhalten und durch28 neue Root-Regressionen gesichert. Drei SDK-Prüfungen wurden unverändert per Hash übernommen. Die vorherige Remove-Abnahme c71 bleibt bestanden.

Aktuell laufen zwei Instanzen mit DLL5a3bf08b6732e2bd03ae7836731814de666e773e0d57e2a819188cb948bc4f0b, Lauf99a11838-fdeb-43ea-8324-108a26447abf, Fixture-Snapshotd0ab4499-602a-4e2d-a68d-4bb5403e4808. Setupf9aeffa5-6214-4232-a6f1-52f3c9d303f8 ist nur erforderliche Vorbereitung, kein erneuter Persistenz-Meilenstein. Integriert: typed_remove_v2, vier hashgebundene Dateien aus review_delta; RootNativeEngineBridge und MCP start_typed_actions/request_snapshot_remove. Korrekturen für boolean Ledger, verspätete Completion, conflictingDuplicateACK/PendingACK sind mit sechs neuen Controller-Tests belegt, ursprüngliche zwölf Tests unverändert erhalten. Native Controller-Start und Client-Auftrag noch nicht ausgeführt. Nächster Schritt: neue RootBridge-/SDK-Units abschließen, einmal Client-Auftrag ->Host ->Client ->beide RetirementACKs nativ beweisen.

Ownership: Root Produktion/Build/MCP/NativeProof; free_mcp_tests nur tools/typed_action_tests; action_session nur tools/action_session/ui_capture; engine_host_delta nur tools/engine_analysis/object_identity/build_actions/place/runtime. FullPlace-Blueprintgröße ist0x260=608Bytes, kein260Byte-Allocator. Bisher nur56Byte-FreeObserver nativ belegt. UI-Remove-Capture, vollständiger Baucommit/Undo, alle weiteren Aktionen, laufende Wirtschaft/Events/Simulation/Steam bleiben offen. Kein Computer Use, zwei Instanzen; vier und großer Save final.
'''
path.write_text(text[:start]+replacement+text[end:],encoding='utf-8')
print('info: current typed-action checkpoint updated; no game effects')
