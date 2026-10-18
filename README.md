# MMORPG Tycoon 2 Multiplayer - Early Beta

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

Build together in one Host world. This is an early beta in active development. Expect bugs, desyncs and unfinished features. Start with a small world and keep backups of saves you care about.

This repository contains the Windows x64 Multiplayer plugin, build tools and legacy standalone tools. Game files, saves, personal settings and native test artifacts are excluded.

## Install with Vortex

Install MT2 Mod Framework Runtime 0.1.6 and the MMORPG Tycoon 2 Vortex Extension 0.1.2 once, using the matching downloads listed with the Multiplayer release. Both players need the same Multiplayer version and dependencies.

1. Close the game and import the Multiplayer ZIP into Vortex for MMORPG Tycoon 2.
2. Enable Multiplayer and deploy.
3. Start the game through Steam.
4. Double-click Multiplayer on the game desktop, or choose Mods... > Multiplayer > Open while in a world.
5. Host a lobby and create or load a small world, or join your friend's lobby.

Vortex manages the files and profile. No manual profile path or configuration edit is required. Close the game before updating, disabling or removing this native plugin. Keep your saves and profile data.

Use the downloads supplied together with the release. Do not combine the Framework plugin with an older standalone Multiplayer loader or Steam proxy. Historical standalone packages are documented in the [beta guide](docs/BETA.md).

## Build and test

Building requires Windows x64, Visual Studio 2022 with C++, the Windows SDK and MASM, CMake, and Python 3. The mod build needs an import library matching the included SDL 3.2.24 headers. CI builds that library from pinned official SDL source without using a game DLL.

```powershell
cmake -S ci -B build/unit -A x64
cmake --build build/unit --config Release --parallel 2
ctest --test-dir build/unit -C Release --output-on-failure --no-tests=error
python -m unittest discover -s test -p test_build_input_guard.py
```

The [Windows workflow](.github/workflows/windows.yml) runs the game-independent tests and builds `MT2_Multiplayer`, `wsock32`, `steam_api64` and `mt2mp_sidecar`. It does not launch the game or load the mod DLL. A successful build does not prove gameplay behavior.

Once the SDL3 import library is available at `include/vendor/SDL3/lib/x64/SDL3.lib`:

```powershell
python -X utf8 tools/build.py --mode framework --framework-sdk C:/path/to/ModFramework --version 0.1.0-beta.14 --target MT2MultiplayerPlugin
```

Framework mode needs a matching Framework source checkout with the native service, route and ownership headers. It writes `build/framework/bin/Release/MT2MultiplayerPlugin.dll` and does not build a second loader or MinHook runtime. It uses a separate build directory from the standalone tools. Source, build recipe, assembly and consumed SDK headers are checked for changes during compilation.

The Framework version defaults to `mod.json`; an explicit version must match it. A matching CMake cache skips the redundant configure step while retaining CMake's automatic regeneration. Use `--reconfigure` to configure explicitly. The build writes `build/framework/build-receipt.json` with the exact DLL hash and version.

Package that local plugin with the canonical manifest:

```powershell
python -X utf8 tools/package.py --mode vortex --build build/framework/bin/Release --freeze build/framework/build-receipt.json --multiplayer-package . --output artifacts/Multiplayer-local.zip
```

Packaging reads back every ZIP entry and rejects a changed DLL or mismatched manifest version. Keep a copy of a package before the next build if you need its exact bytes later.

To build the legacy standalone tools:

```powershell
cmake -S . -B build/mod -A x64 -DMT2MP_BUILD_TESTS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/mod --config Release --target MT2_Multiplayer wsock32 steam_api64 mt2mp_sidecar --parallel 2
```

The separate CI test entry avoids linking every test to the injected mod DLL. Launcher and MCP tools require a legally installed copy of the game and explicit local runtime paths. CI does not run these tools.

## Dependencies

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for MinHook, Dear ImGui, nlohmann/json and SDL. The Steam declarations are a small local interface stub; the Steamworks SDK and original Steam DLLs are not included.

The source-check workflow has read-only access to repository contents. The beta workflow builds and checks tags such as `v0.1.0-beta.1`, then publishes a prerelease with a ZIP and SHA256 file. Only its release job has write permission. Mod-authored code, documentation and assets are covered by the [project license](LICENSE.md). Third-party components retain their own licenses.
