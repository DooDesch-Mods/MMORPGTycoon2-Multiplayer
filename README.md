# MMORPG Tycoon 2 Multiplayer

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

An experimental cooperative multiplayer mod for MMORPG Tycoon 2 on Windows. This private repository contains the mod source and build tools. It is not a finished release. Targeted local gameplay checks do not establish complete online or Steam support.

The repository includes the mod DLL, installed Steam loader, native multiplayer UI, and separate proxy/relay test tools. Game installations, original game libraries, saves, personal settings and native test artifacts are excluded.

## Build and test

Building requires Windows x64, Visual Studio 2022 with C++, the Windows SDK and MASM, CMake, and Python 3. The mod build needs an import library matching the included SDL 3.2.24 headers. CI builds that library from pinned official SDL source without using a game DLL.

```powershell
cmake -S ci -B build/unit -A x64
cmake --build build/unit --config Release --parallel 2
ctest --test-dir build/unit -C Release --output-on-failure
python -m unittest discover -s test -p test_build_input_guard.py
```

The [Windows workflow](.github/workflows/windows.yml) runs the game-independent tests and builds `MT2_Multiplayer`, `wsock32`, `steam_api64` and `mt2mp_sidecar`. It does not launch the game or load the mod DLL. A successful build does not prove gameplay behavior.

Once the SDL3 import library is available at `include/vendor/SDL3/lib/x64/SDL3.lib`:

```powershell
cmake -S . -B build/mod -A x64 -DMT2MP_BUILD_TESTS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/mod --config Release --target MT2_Multiplayer wsock32 steam_api64 mt2mp_sidecar --parallel 2
```

The separate CI test entry avoids linking every test to the injected mod DLL. Launcher and MCP tools require a legally installed copy of the game and explicit local runtime paths. CI does not run these tools.

## Installation status

New drop-in beta packages contain `WSOCK32.dll`, `MT2_Multiplayer.dll` and the authored `mt2mp_ui` folder. Copy those beside `MT2.exe` and launch normally through Steam. Keep the game's original `steam_api64.dll`; an older proxy copied into that location requires the hash-checked restoration in the [beta guide](docs/BETA.md). Do not overwrite another mod's unknown loader.

Multiplayer worlds and diagnostics use a separate private profile. The current Steam account's normal saves also appear on the desktop and in the world picker. The mod reads those originals and writes session saves into its private profile. Existing normal saves and portable test directories are preserved. Historical beta.2 packages use their separate portable starter; keep those files outside the game installation.

World selection, desktop icon dragging and native double-click behavior have passed targeted checks in the game. Joining from another computer, world transfer and shared online gameplay still require acceptance with separate Steam accounts.

Download private test builds from [GitHub Releases](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/releases). See the [beta guide](docs/BETA.md) for tester steps, known limitations and the tagged release workflow.

## Dependencies

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for MinHook, Dear ImGui, nlohmann/json and SDL. The Steam declarations are a small local interface stub; the Steamworks SDK and original Steam DLLs are not included.

The source-check workflow has read-only access to repository contents. The beta workflow builds and checks tags such as `v0.1.0-beta.1`, then publishes a prerelease with a ZIP and SHA256 file. Only its release job has write permission. Mod-authored code, documentation and assets are covered by the [project license](LICENSE.md). Third-party components retain their own licenses.
