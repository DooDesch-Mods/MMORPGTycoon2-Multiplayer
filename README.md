# MMORPG Tycoon 2 Multiplayer

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

An experimental cooperative multiplayer mod for MMORPG Tycoon 2 on Windows. This private repository contains the mod source and build tools. It is not a finished release. Targeted local gameplay checks do not establish complete online or Steam support.

The repository includes source for the mod DLL, Steam API proxy, local relay and native multiplayer UI. Game installations, original game libraries, saves, personal settings and native test artifacts are excluded.

## Build and test

Building requires Windows x64, Visual Studio 2022 with C++, the Windows SDK and MASM, CMake, and Python 3. The mod build needs an import library matching the included SDL 3.2.24 headers. CI builds that library from pinned official SDL source without using a game DLL.

```powershell
cmake -S ci -B build/unit -A x64
cmake --build build/unit --config Release --parallel 2
ctest --test-dir build/unit -C Release --output-on-failure
python -m unittest discover -s test -p test_build_input_guard.py
```

The [Windows workflow](.github/workflows/windows.yml) runs the game-independent tests and builds `MT2_Multiplayer`, `steam_api64` and `mt2mp_sidecar`. It does not launch the game or load the mod DLL. A successful build does not prove gameplay behavior.

Once the SDL3 import library is available at `include/vendor/SDL3/lib/x64/SDL3.lib`:

```powershell
cmake -S . -B build/mod -A x64 -DMT2MP_BUILD_TESTS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/mod --config Release --target MT2_Multiplayer steam_api64 mt2mp_sidecar --parallel 2
```

The separate CI test entry avoids linking every test to the injected mod DLL. Launcher and MCP tools require a legally installed copy of the game and explicit local runtime paths. CI does not run these tools.

## Installation status

The intended installation is to extract the mod into the game directory and launch normally through Steam. That distribution path is still being implemented. The current private test package uses an isolated runtime and is not the final installation method.

World selection has been verified in the native menu. Native desktop icon integration is being tested. Joining from another computer, world transfer and shared online gameplay still require acceptance with separate Steam accounts.

## Dependencies

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for MinHook, Dear ImGui, nlohmann/json and SDL. The Steam declarations are a small local interface stub; the Steamworks SDK and original Steam DLLs are not included.

The workflow has read-only access to repository contents. It does not create releases or publish packages. This initial private source import does not assign a new open-source license to mod-authored code.
