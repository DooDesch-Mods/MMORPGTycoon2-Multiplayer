from __future__ import annotations

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    source = (args.source / "src/game/typed_action_runtime.cpp").read_text(encoding="utf-8")
    function = source.split("bool enable_live_state(std::string& error) {", 1)[1].split("\nbool read_character_membership_actors(", 1)[0]
    start = "    runtime->live_port=std::make_unique<live_host_state::NativePort>"
    tail = function.split(start, 1)[1].rsplit("\n}", 1)[0]
    tail = start + tail
    assert function.count('startup_timing::Phase measured("live_state_setup",error);') == 1
    timed_calls = (
        ("day_bootstrap", "runtime->day_state->bootstrap(error)"),
        ("party_bind", "live_state->bind_parties(*runtime->parties,error)"),
        ("vip_bind", "live_state->bind_vip(*runtime->vip_presentation,error)"),
        ("release_bind", "live_state->bind_release(*runtime->release,error)"),
        ("management_bind", "live_state->bind_management(*runtime->management,error)"),
        ("marketing_bind", "live_state->bind_marketing(*runtime->marketing,error)"),
        ("monster_bind", "live_state->bind_monsters(npc_probe::monster_action_runtime::live_port(),error)"),
    )
    # Extract only the exact checked wrappers. The fixture exercises the original
    # publication and failure paths without substituting a timing implementation.
    for phase, call in timed_calls:
        wrapped = f'startup_timing::run("{phase}",error,[&]{{return {call};}})'
        guarded = f"if(!{wrapped})return false;"
        assert tail.count(guarded) == 1, f"timed provider failure guard changed: {phase}"
        tail = tail.replace(guarded, f"if(!{call})return false;", 1)
    assert "startup_timing::" not in tail, "unexpected timing wrapper in publication tail"
    publication = "runtime->engine.bind_live_state(*runtime->live_state,*runtime->live_port);"
    assert tail.count(publication + "return measured.complete(true);") == 1
    tail = tail.replace(publication + "return measured.complete(true);", publication + "return true;", 1)
    ordered = (
        "auto live_state=std::make_unique<live_host_state::Bridge>",
        "bootstrap(error)",
        "live_state->bind_parties(",
        "live_state->bind_vip(",
        "live_state->bind_release(",
        "live_state->bind_management(",
        "live_state->bind_marketing(",
        "live_state->bind_monsters(",
        "runtime->live_state=std::move(live_state);",
        "runtime->engine.bind_live_state(*runtime->live_state,*runtime->live_port);return true;",
    )
    positions = [tail.index(needle) for needle in ordered]
    assert positions == sorted(positions), "live publication order changed"
    assert tail.count("runtime->live_state") == 2, "unexpected early shared Bridge access"
    assert tail.count("return false") == 7, "fallible bootstrap/provider count changed"
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "actual_live_start_tail.h").write_text(
        "bool actual_live_start_tail(Runtime* runtime, action::Context context, "
        "Binding binding, int epoch, Fence& fence, std::string& error) {\n"
        + tail + "\n}\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
