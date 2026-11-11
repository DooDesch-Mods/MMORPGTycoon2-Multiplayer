# MMORPG Tycoon 2 Multiplayer

Native Multiplayer plugin (`MT2MultiplayerPlugin.dll`) for the MT2 Mod Framework. Players read
[README.md](README.md); builds and tests are in [docs/BUILDING.md](docs/BUILDING.md) and
[docs/TESTING.md](docs/TESTING.md).

## Layout

- `mod.json` - plugin id and the only copy of the version; CMake, packaging and the release tag read it.
- `src/game/` game integration, shared actions, UI. `src/net/` transport and sync. `src/core/` plugin entry, profile, config.
- `cmake/framework_plugin.cmake` - the only product (plus the `mt2mp_sidecar` relay for the local two-client test).
- `test/`, `ci/` - game-independent tests and their CMake. `tools/mcp`, `tools/e2e` - MCP bridge and scripted game runs.
- `.github/framework-sdk.json` - the only Framework pin (a tag). CI, `cmake/framework_pin.cmake` and packaging read it; the pinned SDK also supplies MinHook, nlohmann/json and SDL3.
- `listing/` - Nexus page text; updated on Nexus by hand, never from CI.

## Commands

- SDK: `git clone --depth 1 --branch <pinned tag> https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework build/framework-sdk`.
- Plugin: `python -X utf8 tools/build.py --framework-sdk build/framework-sdk --native-only` (tests: drop `--native-only`).
- Source tests: `cmake -S ci -B build/unit -A x64 -DMT2_FRAMEWORK_SDK=build/framework-sdk`, build, then `ctest --test-dir build/unit -C Release`.
- Python tests: `pip install -r tools/mcp/requirements.txt`, set `PYTHONPATH=tools/mcp`, then run each `test_*.py` under `test/` and `tools/` in its own process (loop in [docs/BUILDING.md](docs/BUILDING.md)); `test/mcp` fails when discovered as one directory.
- Package: `python -X utf8 tools/package.py --build build/framework/bin/Release --freeze build/framework/build-receipt.json --multiplayer-package . --framework-sdk build/framework-sdk --output artifacts/Multiplayer-local.zip`.
- Two local game instances: `tools/framework_local_test.py` (needs a game copy; never touches the installed game folder).

## Traps

- Release: version in `mod.json` plus a `## <version>` section in `CHANGELOG.md`, merge, then push tag `v<version>`. `release.yml` builds, creates the GitHub release and uploads to Nexus. Never publish from a branch.
- Raising the pin raises the minimum Framework Runtime players need.

## Rules

- Documentation and UI text are English. Player instructions use the real UI labels.
- Keep the plugin separate from the Framework loader; use the pinned Framework interfaces, no second loader or hook owner.
- Keep native ownership, thread and version guards and the original error cause. Distinguish an accepted request from a confirmed result; never repeat a world-changing operation whose result is unknown.
- A green build or test does not show in-game behavior. For native behavior use the owned launcher and MCP bridge, isolated profiles and save copies, and state the tested scope (local peers do not prove separate-account online play).
- Keep manifest, package version, tag and `CHANGELOG.md` consistent. Package the exact checked DLL with its build receipt.
- Never commit game files, saves, credentials, profiles, local paths or machine-specific notes. Keep third-party notices and follow [LICENSE.md](LICENSE.md).
