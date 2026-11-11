# MMORPG Tycoon 2 Multiplayer

Native Multiplayer plugin (`MT2MultiplayerPlugin.dll`) for the MT2 Mod Framework. Players read
[README.md](README.md); builds and tests are in [docs/BUILDING.md](docs/BUILDING.md) and
[docs/TESTING.md](docs/TESTING.md).

## Layout

- `mod.json` - plugin id and version; the version must equal the build version.
- `src/game/` game integration, shared actions, UI. `src/net/` transport and sync. `src/core/` plugin entry, profile, config.
- `cmake/framework_plugin.cmake` - the only product (plus the `mt2mp_sidecar` relay for the local two-client test).
- `test/`, `ci/` - game-independent tests and their CMake. `tools/mcp`, `tools/e2e` - MCP bridge and scripted game runs.
- `.github/framework-sdk.json` - pinned Framework SDK revision and header hashes.

## Commands

- Plugin: `python -X utf8 tools/build.py --framework-sdk <sdk> --native-only` (tests: drop `--native-only`).
- Source tests: `cmake -S ci -B build/unit -A x64`, build, then `ctest --test-dir build/unit -C Release`.
- Python tests: the commands in the `python-tests` job of `.github/workflows/windows.yml`; run `pip install -r tools/mcp/requirements.txt` first.
- Package: `python -X utf8 tools/package.py --build build/framework/bin/Release --freeze build/framework/build-receipt.json --multiplayer-package . --output artifacts/Multiplayer-local.zip`.
- Two local game instances: `tools/framework_local_test.py` (needs a game copy; never touches the installed game folder).

## Traps

- Many source tests cut function bodies out of production files by text markers at CMake configure time. Moving or renaming a function can fail `ci/` with "update the affected body test".
- `tools/mcp` shadows the pip package `mcp` when `tools/` is on `PYTHONPATH`; point `PYTHONPATH` at `tools/mcp`.
- `tools/build.py` hard-codes `C:\Program Files\CMake\bin\cmake.exe`.
- Releases are built only from a `v*-beta.*` tag (`beta.yml`); never publish from a branch.

## Rules

- Documentation and UI text are English. Player instructions use the real UI labels.
- Keep the plugin separate from the Framework loader; use the pinned Framework interfaces, no second loader or hook owner.
- Keep native ownership, thread and version guards and the original error cause. Distinguish an accepted request from a confirmed result; never repeat a world-changing operation whose result is unknown.
- A green build or test does not show in-game behavior. For native behavior use the owned launcher and MCP bridge, isolated profiles and save copies, and state the tested scope (local peers do not prove separate-account online play).
- Keep manifest, package version, tag and `CHANGELOG.md` consistent. Package the exact checked DLL with its build receipt.
- Never commit game files, saves, credentials, profiles, local paths or machine-specific notes. Keep third-party notices and follow [LICENSE.md](LICENSE.md).
