from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
old = 'Ein Subagent bündelt zusätzliche Acceptance-Unit-Tests; native Beobachtungen werden dadurch nicht erfunden.'
new = ('36 gezielte [Acceptance-Unit-Tests](acceptance_regressions.md) bestehen. '
       'Sie prüfen Dateikorruption, Identität, Teilquittungen und Wiederholungsschutz. '
       'Native Beobachtungen bleiben separate Belege.')
old_owner = ('Root hat RemoveBackend14 Dateien, PropHost/Pool und NativeFree gelesen; direkte '
             'D0-C++Originalcause, preAssign-Lifetime und Unknown-Cleanup werden vor Freeze korrigiert. '
             'NativeFree-/CRT-/IAT-/Census-/Retirement-Services bleiben echte Integrationsaufgaben. '
             'Netzwerkbau-Reviewdelta und Acceptance-Unit-Tests beim Subagenten.')
new_owner = ('RemoveBackend, PropHost/Pool, NativeFree und die globalen Allocationservices sind gelesen. '
             'Originalcause, preAssign-Lifetime und Unknown-Cleanup sind in versiegelten Kandidaten korrigiert. '
             'Der konkrete CRT-/IAT-Installer und seine gezielte private Holder-MCP-Probe werden eingebaut. '
             'Ein Subagent erstellt den tatsächlichen RemoveRuntimeadapter, ein zweiter die neuen MCP-/Probe-Unit-Tests. '
             '36 Acceptance-Regressionen sind bestanden; unveränderte frühere Prüfungen werden übernommen.')
for name in ('tools/checkpoint_persistence.py', '_drafts/small_persistence.md', '_drafts/implementation_state.md'):
    path = ROOT / name
    text = path.read_text(encoding='utf-8')
    text = text.replace(old, new).replace(old_owner, new_owner)
    path.write_text(text, encoding='utf-8')
path = ROOT / 'tools/update_progress.py'
text = path.read_text(encoding='utf-8').replace(
    'Subagent-Unit-Tests sichern Regressionen.',
    '<a href="acceptance_regressions.md">36 bestandene Unit-Tests</a> sichern Regressionen.')
path.write_text(text, encoding='utf-8')
print('Regression checkpoint updated; native evidence retained.')
