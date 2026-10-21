# Build from source

Building requires Windows x64, Visual Studio 2022 with C++, the Windows SDK and MASM, CMake, and Python 3. The mod build needs an import library matching the included SDL 3.2.24 headers. CI builds that library from pinned official SDL source without using a game DLL.

```powershell
cmake -S ci -B build/unit -A x64
cmake --build build/unit --config Release --parallel 2
ctest --test-dir build/unit -C Release --output-on-failure --no-tests=error
python -m unittest discover -s test -p test_build_input_guard.py
```

The [Windows workflow](../.github/workflows/windows.yml) runs the game-independent tests and builds the native Framework plugin. It does not launch the game or load the plugin. A successful build does not prove gameplay behavior.

Once the SDL3 import library is available at `include/vendor/SDL3/lib/x64/SDL3.lib`:

```powershell
python -X utf8 tools/build.py --mode framework --framework-sdk C:/path/to/ModFramework --version 0.1.0-beta.17 --target MT2MultiplayerPlugin
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

