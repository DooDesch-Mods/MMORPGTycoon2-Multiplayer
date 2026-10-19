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
