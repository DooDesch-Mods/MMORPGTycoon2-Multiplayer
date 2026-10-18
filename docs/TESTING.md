# Native multiplayer tests

Use the existing owned local launcher and MCP bridge. Tests do not use desktop
input, another player's process, or private save profiles.

## Shared New World flow

Start two instances with `tools/framework_local_test.py`. Retain its `run.json`,
the frozen plugin DLL and the Framework DLL checksum. Then run:

```powershell
python -B -X utf8 tools/lobby_ui/creation_acceptance.py `
  --test-root <owned-run-root> `
  --expected-run-id <run-id> `
  --candidate-binary <frozen-plugin-dll> `
  --framework-sha256 <framework-dll-sha256> `
  --desktop-proof docs/evidence/lobby-desktop-native.json `
  --handoff-proof docs/evidence/lobby-creation-handoff-native.json
```

The script connects both peers, opens the Host's original New Game wizard,
selects the required native icon and world type if unset, assigns unallocated
Player Appeal points through the original Plus button, confirms its five
pages, invokes Create once, and waits for both peers to acknowledge the same
snapshot. It also checks the in-game Desktop visibility and Mods entry.
No stdin confirmation is required. Existing selections stay unchanged.

The two optional proof arguments retain previously accepted Desktop gestures
and wizard cancellation behavior. Omit them only when that behavior changed
and needs fresh native acceptance.

Each step waits for an actual game acknowledgement. A timeout or contradictory
mutation result stops the sequence without replaying the action. The retained
acceptance summary identifies the failed step, exact process, response files
and captured game frames under `artifacts/acceptance/`.

The script leaves the owned instances open for inspection. Stop them through
`tools/framework_local_test.py --stop --test-root <launcher-root>`.

## Fast regressions

```powershell
python -B -X utf8 -m unittest discover -s test/lobby_ui -p test_wizard_automation.py
ctest --test-dir build/framework -C Release -R "^lobby_(handoff|wizard_automation)$" --output-on-failure
```

The automation tests exercise the production driver with controlled MCP replies:
exactly-once mutations, preserved selections and allocations, delayed pages,
disabled buttons, map generation, invalid state and uncertain responses. They verify the driver;
the two-instance run proves the actual game behavior. Neither proves separate
Steam accounts, the remaining gameplay action families or acceptable FPS.
