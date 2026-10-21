from pathlib import Path
import argparse

p = argparse.ArgumentParser()
p.add_argument("--source", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
probe = (a.source / "src/game/placement_ui_probe/ui_probe.cpp").read_text()
start = probe.index("Receipt Probe::arm(")
end = probe.index("bool merge_release_admission(", start)
actual = probe[start:end]
assert "Receipt complete_build_input_at_update(" in actual
assert actual.index("step(7,0,0)") < actual.index("if(ok)ok=step(2,0,0)")
assert "receipt.phase=Phase::Attempted;services.record_receipt(r,receipt);" in actual
diagnostic = (a.source / "src/game/placement_ui_diagnostics/placement_ui_diagnostics.cpp").read_text()
entry = diagnostic[diagnostic.index("void build_update_entered("):diagnostic.index("bool probe(")]
observer = entry
for guard in (
    "thread!=GetCurrentThreadId()",
    "active->binding.family==ui::Family::Building",
    "reinterpret_cast<uintptr_t>(receiver)==active->binding.behaviour",
    "active->receipt.phase==ui::Phase::ArmedAwaitingGameUpdate",
    "!(active->receipt.attempted_mask&((1u<<2)|(1u<<3)|(1u<<4)))",
):
    assert guard in entry, guard
runtime = (a.source / "src/game/ui_build_runtime.cpp").read_text()
entry = runtime[runtime.index("void update_entry("):]
assert entry.index("placement_ui_diagnostics::build_update_entered(receiver,dt)") < entry.index("runtime->build.capture_at_update_entry(")
admission = (a.source / "src/game/placement_ui_diagnostics/admission.cpp").read_text()
for coordinate in ("s.x!=r.position[0]", "s.y!=r.position[1]", "s.z!=r.position[2]"):
    assert coordinate in admission
assert "case 7:if(!reinterpret_cast<bool(__fastcall*)(void*,const float*)>(b.context.image+0x5863e0)" in probe
observe=diagnostic[diagnostic.index('bool observe(Job&'):diagnostic.index('class Services final:')]
assert observe.index('observe_confirmation(')<observe.index('may_read_fresh_pose(')<observe.index('read_action_prop(')
receipt=diagnostic[diagnostic.index('bool receipt(const json&'):diagnostic.index('bool release_ready(')]
assert 'can_reconcile_timeout(j)' in receipt and 'auto observed=j;' in receipt
assert receipt.index('actual(observed,context,cause)')<receipt.index('observe(observed,cause)')<receipt.index('publish_reconciliation(j,observed,cause)')
assert 'observation_timeout(j,GetTickCount64())' in receipt
assert 'native_probe.arm' not in receipt and 'complete_build_input_at_update' not in receipt
a.output.mkdir(parents=True, exist_ok=True)
(a.output / "placement_input_order.inc").write_text(actual, encoding="utf-8")
(a.output / "placement_observer_order.inc").write_text(observer, encoding="utf-8")
print("PASS extracted actual arm and normal Update input boundary; receiver, context, exact pose and capture ordering retained")
