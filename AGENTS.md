# Contributor and agent guide

This repository contains the native Multiplayer plugin for MMORPG Tycoon 2.
Read [README.md](README.md) for player-facing behavior and requirements, and
[docs/BUILDING.md](docs/BUILDING.md) for build, test and packaging commands.

## Project map

- `mod.json` defines the plugin identity, version and Framework API contract.
- `src/game/` contains native game integration, shared actions and the Multiplayer UI.
- `src/net/` contains session transport and synchronization.
- `test/` and `ci/` contain game-independent tests and their build definitions.
- `tools/` contains build, packaging and owned-game test tools.
- `.github/framework-sdk.json` pins the Framework SDK used by CI.
- `CHANGELOG.md` describes release changes for players.

## Make a change

Write documentation and development-facing text in English. Use English as the
default UI language. Keep player instructions consistent with actual UI labels.

Keep changes focused on the requested behavior. Preserve native ownership,
thread and version guards, protocol compatibility and original error causes.
Distinguish an accepted request from a confirmed result. Do not automatically
repeat a world-changing operation whose result is unknown.

Keep the Multiplayer plugin separate from the Framework loader. Use the pinned
Framework interfaces rather than introducing a second loader or hook owner.
Native plugins require a game restart after an update.

## Verify the result

Run the tests affected by the change, following the build guide. Update extracted
test fixtures when a real interface changes; preserve their assertions and error
coverage. A successful build does not establish behavior inside the game.

For native behavior, use the existing owned-game launcher and MCP bridge described
in [docs/TESTING.md](docs/TESTING.md). Use isolated test profiles and save copies.
Inspect acknowledgements, logs and real game captures. State the tested scope
accurately; local peers do not establish separate-account online compatibility.
Documentation-only changes do not require replaying unchanged gameplay tests.

Public roadmap stages and feature coverage originate in `docs/roadmap.json`.
Run `python tools/render_roadmap.py` after changing it. Its `--check` mode verifies
the README, text roadmap and desktop/mobile SVGs without writing. Each concrete
feature has one stable ID and one status: released, verified in a development
build, implemented, or planned. Keep its scope and evidence reference current.
Only shipped behavior is released; development confirmation does not change the
download. A successful build alone does not establish in-game confirmation.

## Prepare a release

Keep the manifest, package version, release tag and changelog consistent. Package
the exact checked native binary with its build receipt and checksum. Keep the
Early Beta status clear, and describe player-visible changes in public copy.

Do not commit game files, saves, credentials, personal profiles, local paths or
machine-specific task notes. Keep private work records outside tracked source.
Preserve third-party notices and follow [LICENSE.md](LICENSE.md) before publishing
or redistributing the project.
